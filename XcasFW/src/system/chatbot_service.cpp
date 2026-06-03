#include "system/chatbot_service.h"

#include <cstdio>
#include <cstring>

#include "app_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "chatbot";
constexpr uint32_t kHttpTimeoutMs = 30000;
constexpr size_t kBodyCapacity = 1024;
constexpr size_t kResponseCapacity = 4096;

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

void copy_text(char* output, size_t output_size, const char* input) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  std::snprintf(output, output_size, "%s", input == nullptr ? "" : input);
}

size_t json_escape(const char* input, char* output, size_t output_size) {
  size_t used = 0;
  if (output_size == 0) {
    return 0;
  }
  if (input == nullptr) {
    output[0] = '\0';
    return 0;
  }
  for (const char* p = input; *p != '\0' && used + 2 < output_size; ++p) {
    const char ch = *p;
    if (ch == '"' || ch == '\\') {
      output[used++] = '\\';
      output[used++] = ch;
    } else if (ch == '\n') {
      output[used++] = '\\';
      output[used++] = 'n';
    } else if (static_cast<unsigned char>(ch) >= 0x20) {
      output[used++] = ch;
    }
  }
  output[used] = '\0';
  return used;
}

bool append_unescaped_json_string(char* output,
                                  size_t output_size,
                                  const char* begin,
                                  const char* end) {
  size_t used = 0;
  for (const char* p = begin; p < end && used + 1 < output_size; ++p) {
    if (*p == '\\' && p + 1 < end) {
      ++p;
      if (*p == 'n') {
        output[used++] = ' ';
      } else if (*p == '"' || *p == '\\' || *p == '/') {
        output[used++] = *p;
      }
      continue;
    }
    output[used++] = *p;
  }
  output[used] = '\0';
  return used > 0;
}

bool extract_openai_content(const char* response, char* output, size_t output_size) {
  const char* marker = std::strstr(response, "\"content\"");
  if (marker == nullptr) {
    return false;
  }
  const char* colon = std::strchr(marker, ':');
  if (colon == nullptr) {
    return false;
  }
  const char* begin = std::strchr(colon, '"');
  if (begin == nullptr) {
    return false;
  }
  ++begin;
  const char* end = begin;
  bool escaped = false;
  while (*end != '\0') {
    if (!escaped && *end == '"') {
      return append_unescaped_json_string(output, output_size, begin, end);
    }
    escaped = !escaped && *end == '\\';
    if (*end != '\\') {
      escaped = false;
    }
    ++end;
  }
  return false;
}

}  // namespace

esp_err_t ChatbotService::start(QueueHandle_t app_events, WifiManager& wifi) {
  if (requests_ != nullptr) {
    return ESP_OK;
  }
  app_events_ = app_events;
  wifi_ = &wifi;
  requests_ = xQueueCreateStatic(1, sizeof(Request), request_buffer_, &request_storage_);
  if (requests_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }
  const BaseType_t ok = xTaskCreatePinnedToCore(&ChatbotService::task_entry,
                                                "chatbot",
                                                12 * 1024,
                                                this,
                                                4,
                                                nullptr,
                                                config::kUiCore);
  return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t ChatbotService::ask(const ChatbotSettings& settings, const char* prompt) {
  if (requests_ == nullptr || !settings.loaded || !has_text(prompt)) {
    return ESP_ERR_INVALID_STATE;
  }
  Request request {};
  request.settings = settings;
  copy_text(request.prompt, sizeof(request.prompt), prompt);
  return xQueueSend(requests_, &request, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void ChatbotService::task_entry(void* arg) {
  static_cast<ChatbotService*>(arg)->task_loop();
}

void ChatbotService::task_loop() {
  Request request {};
  while (true) {
    if (xQueueReceive(requests_, &request, portMAX_DELAY) == pdTRUE) {
      handle_request(request);
    }
  }
}

void ChatbotService::handle_request(const Request& request) {
  if (wifi_ == nullptr || !wifi_->wait_connected(10000)) {
    publish(false, "WiFi not connected");
    return;
  }

  char escaped_prompt[256] {};
  char escaped_system[256] {};
  json_escape(request.prompt, escaped_prompt, sizeof(escaped_prompt));
  json_escape(has_text(request.settings.system_prompt)
                  ? request.settings.system_prompt
                  : "You are a concise calculator assistant.",
              escaped_system,
              sizeof(escaped_system));

  char body[kBodyCapacity] {};
  std::snprintf(body,
                sizeof(body),
                "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
                "{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":160}",
                has_text(request.settings.model) ? request.settings.model : "gpt-4.1-mini",
                escaped_system,
                escaped_prompt);

  esp_http_client_config_t config {};
  config.url = request.settings.endpoint;
  config.timeout_ms = kHttpTimeoutMs;
  config.skip_cert_common_name_check = true;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    publish(false, "HTTP init failed");
    return;
  }

  char auth[160] {};
  std::snprintf(auth, sizeof(auth), "Bearer %s", request.settings.api_key);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_header(client, "Authorization", auth);
  esp_http_client_set_post_field(client, body, std::strlen(body));

  char response[kResponseCapacity] {};
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    const int read_len = esp_http_client_read_response(client, response, sizeof(response) - 1);
    if (read_len > 0) {
      response[read_len] = '\0';
    }
  }

  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK) {
    char error[80] {};
    std::snprintf(error, sizeof(error), "HTTP error: %s", esp_err_to_name(err));
    publish(false, error);
    return;
  }
  if (status < 200 || status >= 300) {
    char error[80] {};
    std::snprintf(error, sizeof(error), "API status %d", status);
    publish(false, error);
    return;
  }

  char answer[192] {};
  if (!extract_openai_content(response, answer, sizeof(answer))) {
    copy_text(answer, sizeof(answer), "No answer text in response");
  }
  publish(true, answer);
}

void ChatbotService::publish(bool ok, const char* text) {
  if (app_events_ == nullptr) {
    return;
  }
  AppEvent event {};
  event.type = AppEventType::Chatbot;
  event.chatbot.ok = ok;
  copy_text(event.chatbot.text, sizeof(event.chatbot.text), text);
  xQueueSend(app_events_, &event, 0);
  ESP_LOGI(TAG, "chatbot result queued ok=%d", ok);
}

}  // namespace esp32calc_alt
