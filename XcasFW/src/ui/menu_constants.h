#pragma once

#include <cstddef>
#include <cstdint>

namespace esp32calc_alt::menu_constants {

struct ScientificConstant {
  const char* label;
  const char* value;
  const char* category;
};

struct ScientificConstantGroup {
  const char* label;
  const char* hint;
  const char* category;
};

inline constexpr const char* kLogTag = "alt_ui";

inline constexpr size_t kExpressionCapacity = 128;
inline constexpr size_t kResultCapacity = 192;
inline constexpr size_t kVisibleExpressionChars = 38;
inline constexpr size_t kVisibleResultChars = 36;
inline constexpr size_t kModeStorageSize = 32;
inline constexpr size_t kModeStorageAlign = alignof(std::max_align_t);

inline constexpr int kCharWidth = 6;
inline constexpr int kInputX = 5;
inline constexpr int kInputY = 22;
inline constexpr int kInputWidth = 240;
inline constexpr int kInputHeight = 27;
inline constexpr int kInputTextX = 10;
inline constexpr int kInputTextY = 35;

inline constexpr const char* kVariableTokens[] = {"x", "y", "z", "a", "b", "c"};
inline constexpr size_t kVariableCount = sizeof(kVariableTokens) / sizeof(kVariableTokens[0]);

inline constexpr const char* kModeLabels[] = {"STANDARD", "GRAPH", "CONST", "INTEGRALS"};
inline constexpr const char* kModeHints[] = {"CAS", "PLOT", "SCI", "INT"};
inline constexpr size_t kModeCount = sizeof(kModeLabels) / sizeof(kModeLabels[0]);
inline constexpr int kModeSelectorX = 8;
inline constexpr int kModeSelectorY = 30;
inline constexpr int kModeSelectorRowHeight = 16;
inline constexpr int kModeSelectorRowWidth = 232;

inline constexpr size_t kGraphExpressionCapacity = 128;
inline constexpr int kGraphPoints = 96;
inline constexpr size_t kGraphLabelChars = 28;
inline constexpr float kGraphXMin = -5.0f;
inline constexpr float kGraphXMax = 5.0f;
inline constexpr float kGraphYMin = -5.0f;
inline constexpr float kGraphYMax = 5.0f;
inline constexpr int kGraphX = 10;
inline constexpr int kGraphY = 42;
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kEuler = 2.71828182845904523536f;
inline constexpr float kRelationYMin = -50.0f;
inline constexpr float kRelationYMax = 50.0f;
inline constexpr float kRelationStep = 0.5f;
inline constexpr float kRelationRootEpsilon = 0.0001f;
inline constexpr float kImplicitNearZero = 0.055f;
inline constexpr size_t kExpandedExpressionCapacity = 224;
inline constexpr char kConstantMarker = '@';

inline constexpr size_t kConstantsVisibleRows = 6;
inline constexpr int kConstantsListX = 7;
inline constexpr int kConstantsListY = 29;
inline constexpr int kConstantsRowHeight = 13;
inline constexpr int kConstantsRowWidth = 238;

// fx-991CW / fx-991SP CW style scientific constants. Values follow CODATA 2022
// where applicable; exact conventional values are represented exactly as text.
inline constexpr ScientificConstant kScientificConstants[] = {
    {"pi", "3.141592653589793", "Math"},
    {"e", "2.718281828459045", "Math"},
    {"h", "6.62607015E-34", "Universal"},
    {"hbar", "1.054571817E-34", "Universal"},
    {"c", "299792458", "Universal"},
    {"eps0", "8.8541878188E-12", "Universal"},
    {"mu0", "1.25663706127E-6", "Universal"},
    {"Z0", "376.730313412", "Universal"},
    {"G", "6.67430E-11", "Universal"},
    {"lP", "1.616255E-35", "Universal"},
    {"tP", "5.391247E-44", "Universal"},
    {"muN", "5.0507837393E-27", "Electromag"},
    {"muB", "9.2740100657E-24", "Electromag"},
    {"qe", "1.602176634E-19", "Electromag"},
    {"Phi0", "2.067833848E-15", "Electromag"},
    {"G0", "7.748091729E-5", "Electromag"},
    {"KJ", "4.835978484E14", "Electromag"},
    {"RK", "25812.80745", "Electromag"},
    {"mp", "1.67262192595E-27", "Atomic"},
    {"mn", "1.67492750056E-27", "Atomic"},
    {"me", "9.1093837139E-31", "Atomic"},
    {"mmu", "1.883531627E-28", "Atomic"},
    {"a0", "5.29177210544E-11", "Atomic"},
    {"alpha", "7.2973525643E-3", "Atomic"},
    {"re", "2.8179403205E-15", "Atomic"},
    {"lambdaC", "2.42631023538E-12", "Atomic"},
    {"gammap", "2.6752218744E8", "Atomic"},
    {"lambdaCp", "1.32140985539E-15", "Atomic"},
    {"lambdaCn", "1.31959090581E-15", "Atomic"},
    {"Rinf", "10973731.568157", "Atomic"},
    {"mup", "1.41060679736E-26", "Atomic"},
    {"mue", "-9.2847646917E-24", "Atomic"},
    {"mun", "-9.6623653E-27", "Atomic"},
    {"mumu", "-4.49044830E-26", "Atomic"},
    {"mtau", "3.16754E-27", "Atomic"},
    {"mu", "1.66053906892E-27", "Phys-Chem"},
    {"F", "96485.33212", "Phys-Chem"},
    {"NA", "6.02214076E23", "Phys-Chem"},
    {"k", "1.380649E-23", "Phys-Chem"},
    {"Vm", "2.271095464E-2", "Phys-Chem"},
    {"R", "8.31446261815324", "Phys-Chem"},
    {"c1", "3.741771852E-16", "Phys-Chem"},
    {"c2", "1.438776877E-2", "Phys-Chem"},
    {"sigma", "5.670374419E-8", "Phys-Chem"},
    {"gn", "9.80665", "Adopted"},
    {"atm", "101325", "Adopted"},
    {"RK90", "25812.807", "Adopted"},
    {"KJ90", "4.835979E14", "Adopted"},
    {"t", "273.15", "Other"},
};
inline constexpr size_t kScientificConstantCount =
    sizeof(kScientificConstants) / sizeof(kScientificConstants[0]);

inline constexpr ScientificConstantGroup kScientificConstantGroups[] = {
    {"MATH", "pi/e", "Math"},
    {"UNIVERSAL", "h/c/G", "Universal"},
    {"ELECTROMAG", "mu/q", "Electromag"},
    {"ATOMIC", "m/a0", "Atomic"},
    {"PHYS-CHEM", "NA/R", "Phys-Chem"},
    {"ADOPTED", "g/atm", "Adopted"},
    {"OTHER", "misc", "Other"},
};
inline constexpr size_t kScientificConstantGroupCount =
    sizeof(kScientificConstantGroups) / sizeof(kScientificConstantGroups[0]);

const ScientificConstant* find_scientific_constant(const char* label, size_t length);
bool expand_scientific_constants(const char* input, char* output, size_t output_size);
const ScientificConstantGroup& constant_group_at(size_t index);
bool scientific_constant_matches(const ScientificConstant& item,
                                 size_t index,
                                 uint8_t group,
                                 const char* query);
size_t filtered_scientific_constant_count(uint8_t group, const char* query);
int filtered_scientific_constant_index(uint8_t group, const char* query, size_t ordinal);
int first_scientific_constant_for_group(uint8_t group);
void scientific_constant_code(uint8_t group, size_t index, char* output, size_t output_size);
void sanitize_constant_search_token(const char* token, char* output, size_t output_size);

}  // namespace esp32calc_alt::menu_constants
