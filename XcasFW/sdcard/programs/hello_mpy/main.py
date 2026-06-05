# Interactive app with terminal text output.
# print() → display. handle_key(token) → key input.
# Keymap in keymap.ini maps physical keys to token strings.

print("Test!")

count = 0

def handle_key(token):
    global count
    count = count + 1
    print("[" + str(count) + "] " + token)
