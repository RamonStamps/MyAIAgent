#include <stdio.h>
#include <stdlib.h> // For dynamic memory allocation
#include <string.h>  // For string manipulation
#include <stdbool.h> // For true/false macros
#include <ctype.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

// forward declaration
const char* strcasestr_local(const char* haystack, const char* needle);

// Ollama support: default model name
// Default to a smaller Ollama model to reduce memory and startup time.
// Users can change this with `setmodel` or list available models with `listmodels`.
static char g_ollama_model[128] = "llama2-mini";

// Check if a given Ollama model is installed locally (returns 1 if installed)
int ollama_model_installed(const char* model) {
  if (!model || !model[0]) return 0;
  #ifdef _WIN32
  FILE* p = _popen("ollama list 2>nul", "r");
  #else
  FILE* p = popen("ollama list 2>/dev/null", "r");
  #endif
  if (!p) return 0;
  char buf[1024];
  int found = 0;
  while (fgets(buf, sizeof(buf), p)) {
    if (strstr(buf, model)) { found = 1; break; }
  }
  #ifdef _WIN32
  _pclose(p);
  #else
  pclose(p);
  #endif
  return found;
}

// Parse size string tokens (number + unit) into bytes. Returns -1 on parse error.
long long size_tokens_to_bytes(const char* numTok, const char* unitTok) {
  if (!numTok || !unitTok) return -1;
  double v = atof(numTok);
  if (v <= 0) return -1;
  if (strcasestr_local(unitTok, "GB")) return (long long)(v * 1024.0 * 1024.0 * 1024.0);
  if (strcasestr_local(unitTok, "MB")) return (long long)(v * 1024.0 * 1024.0);
  if (strcasestr_local(unitTok, "KB")) return (long long)(v * 1024.0);
  // If unit not recognized, try plain bytes
  return (long long)v;
}

// Choose the smallest installed Ollama model and set it as default. Returns 1 if set, 0 otherwise.
int ollama_pick_smallest_model() {
  #ifdef _WIN32
  FILE* p = _popen("ollama list 2>nul", "r");
  #else
  FILE* p = popen("ollama list 2>/dev/null", "r");
  #endif
  if (!p) return 0;
  char line[2048];
  long long bestSize = -1;
  char bestName[256] = {0};
  while (fgets(line, sizeof(line), p)) {
    // split tokens by whitespace
    char* toks[16]; int t = 0;
    char* s = line;
    while (*s && t < 16) {
      while (*s && isspace((unsigned char)*s)) s++;
      if (!*s) break;
      toks[t++] = s;
      while (*s && !isspace((unsigned char)*s)) s++;
      if (*s) { *s = '\0'; s++; }
    }
    if (t < 4) continue; // need at least name,id,size,unit
    char* name = toks[0];
    // skip embedding-only models (they return vectors instead of text)
    if (strcasestr_local(name, "embed") || strcasestr_local(name, "embedding")) continue;
    char* sizeNum = toks[2];
    char* sizeUnit = toks[3];
    long long bytes = size_tokens_to_bytes(sizeNum, sizeUnit);
    if (bytes < 0) continue;
    if (bestSize < 0 || bytes < bestSize) {
      bestSize = bytes;
      strncpy(bestName, name, sizeof(bestName)-1);
    }
  }
  #ifdef _WIN32
  _pclose(p);
  #else
  pclose(p);
  #endif
  if (bestSize >= 0 && bestName[0]) {
    strncpy(g_ollama_model, bestName, sizeof(g_ollama_model)-1);
    g_ollama_model[sizeof(g_ollama_model)-1] = '\0';
    return 1;
  }
  return 0;
}

// Print available local Ollama models
void cmd_listmodels() {
  #ifdef _WIN32
  FILE* p = _popen("ollama list 2>nul", "r");
  #else
  FILE* p = popen("ollama list 2>/dev/null", "r");
  #endif
  if (!p) { printf("Failed to list ollama models or ollama not installed.\n"); return; }
  char buf[1024];
  printf("Installed Ollama models:\n");
  int any = 0;
  while (fgets(buf, sizeof(buf), p)) {
    printf("  %s", buf);
    any = 1;
  }
  if (!any) printf("  (none)\n");
  #ifdef _WIN32
  _pclose(p);
  #else
  pclose(p);
  #endif
}

// Define Agent State
typedef struct {
  int state;          // Represents the agent's visual processing (e.g., "scanning," "analyzing")
  int action;         // The action to take (e.g., "move_left", "rotate_right")
  int reward;        // Numerical score for the action's success or failure
} AgentState;

// Function to get Next State based on Current State and Action
AgentState getNextState(AgentState currentState, int action) {
  // Simplified example - expand this!
  if (currentState.state == 0) { // Starting state
    currentState.action = action;
    return currentState;
  } else if (currentState.state == 1) {
    currentState.action = action;
    return currentState;
  } else {
    // Default to a "fallback" state. Important!
    currentState.state = 2;
    currentState.action = action;
    return currentState;
  }
}

// Action Executor Function
int executeAction(AgentState currentState, int action) {
  // Replace with your AI logic here! This is where you'd implement the decision-making.
  printf("Executing action: %d\n", action); // Placeholder - replace!
  return 0;  // Return a default value for actions
}

// Feedback Manager (simplified)
void updateState(AgentState* currentState, int action) {
    *currentState = getNextState(*currentState, action);
}


// File System Helper Functions
int createFile(const char* filename) {  //Placeholder
  printf("Creating file: %s\n", filename);
  return 0; //Return 0 for success - this will be overwritten by the system
}

void openFile(const char* filename) {
    printf("Opening file: %s\n", filename);
}


void deleteFile(const char* filename) {
  printf("Deleting file: %s\n", filename);
}




// Simple helper: print a command list
void printHelp() {
  printf("\nAI Agent Commands:\n");
  printf("  help                      - Show this help\n");
  printf("  open <file>               - Print file contents\n");
  printf("  search <pattern>          - Search files in current directory (.c,.h,.txt)\n");
  printf("  compile <source.c>        - Compile C source with gcc\n");
  printf("  run <executable>          - Run an executable in workspace\n");
  printf("  template <language> <id>  - Show small code templates (c hello/file)\n");
  printf("  chat <message>            - Send a message to model (requires API key in keys/ unless using ollama)\n");
  printf("  setmodel <model>          - Set Ollama model name (default: llama2)\n");
  printf("  listmodels                - List installed local Ollama models\n");
  printf("  complete <file> <line> <col> - Request code completion for file at position\n");
  printf("  usekey <provider>          - Prefer key for provider (ollama,qwen7b,progassist)\n");
  printf("  setkey <provider> <key>    - Provide API key for provider (sent securely via stdin)\n");
  printf("  exit                      - Quit agent\n");
}

// set key: write to keys/<provider>.key with restrictive permissions where possible
void cmd_setkey(const char* provider, const char* key) {
  if (!provider || !provider[0]) { printf("setkey: missing provider\n"); return; }
  if (!key) key = "";
  char safe[256]; int si = 0;
  for (const char* p = provider; *p && si < (int)sizeof(safe)-1; ++p) {
    char c = *p; if (isalnum((unsigned char)c) || c=='-'||c=='_'||c=='.') safe[si++]=c; else safe[si++]='_';
  }
  safe[si]='\0';
  char path[1024]; snprintf(path, sizeof(path), "keys/%s.key", safe);
  // Ensure keys dir exists
  #ifdef _WIN32
  // Windows: use mkdir
  system("if not exist keys mkdir keys >nul 2>nul");
  #else
  system("mkdir -p keys 2>/dev/null");
  #endif
  FILE* f = fopen(path, "wb");
  if (!f) { printf("Failed to open key file %s for writing\n", path); return; }
  fwrite(key, 1, strlen(key), f);
  fclose(f);
  // try to set restrictive permissions on POSIX
  #ifndef _WIN32
  chmod(path, S_IRUSR | S_IWUSR);
  #endif
  printf("Saved key for %s\n", provider);
}

// Print file contents (safely)
void cmd_open(const char* filename) {
  FILE* f = fopen(filename, "r");
  if (!f) {
    printf("Could not open %s\n", filename);
    return;
  }
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    fputs(line, stdout);
  }
  fclose(f);
}

// Forward declaration for runCommandCapture used by chat/complete helpers
void runCommandCapture(const char* cmd);

// API key handling: try to load a key from the keys folder
static char g_api_key[1024] = {0};
static char g_preferred_provider[128] = {0};

// Case-insensitive substring search
const char* strcasestr_local(const char* haystack, const char* needle) {
  if (!*needle) return haystack;
  for (; *haystack; ++haystack) {
    const char *h = haystack, *n = needle;
    while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { ++h; ++n; }
    if (!*n) return haystack;
  }
  return NULL;
}

// case-insensitive equality
int iequals(const char* a, const char* b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    a++; b++;
  }
  return *a == '\0' && *b == '\0';
}

// Escape minimal shell arg for double-quoted Windows/CMD or general shells
char* escape_shell_arg(const char* s) {
  size_t len = strlen(s);
  // worst case every char escaped
  char* out = malloc(len * 2 + 3);
  if (!out) return NULL;
  char* d = out;
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') { *d++ = '\\'; *d++ = *p; }
    else if (*p == '\n') { *d++ = '\\'; *d++ = 'n'; }
    else *d++ = *p;
  }
  *d = '\0';
  return out;
}

// Check whether `ollama` CLI is available on PATH
int ollama_available() {
  #ifdef _WIN32
  FILE* p = _popen("ollama --version 2>nul", "r");
  #else
  FILE* p = popen("ollama --version 2>/dev/null", "r");
  #endif
  if (!p) return 0;
  char buf[256];
  int ok = 0;
  if (fgets(buf, sizeof(buf), p)) ok = 1;
  #ifdef _WIN32
  _pclose(p);
  #else
  pclose(p);
  #endif
  return ok;
}

// Read whole file into buffer (caller must free)
char* readFileToString(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* buf = (char*)malloc(sz + 1);
  if (!buf) { fclose(f); return NULL; }
  if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
  buf[sz] = '\0';
  fclose(f);
  return buf;
}

// Try common key filenames, otherwise scan keys/ folder for first file
void load_api_key() {
  if (g_api_key[0]) return; // already loaded
  const char* candidates[] = {"keys/openai.key", "keys/api_key", "keys/key.txt", NULL};
  for (int i = 0; candidates[i]; ++i) {
    char* s = readFileToString(candidates[i]);
    if (s) {
      // trim whitespace
      char* p = s; while (*p && isspace((unsigned char)*p)) p++;
      char* q = p + strlen(p) - 1; while (q>p && isspace((unsigned char)*q)) *q--='\0';
      strncpy(g_api_key, p, sizeof(g_api_key)-1);
      free(s);
      return;
    }
  }
  // scan keys directory
  #ifdef _WIN32
  FILE* p = popen("dir /b keys 2>nul", "r");
  #else
  FILE* p = popen("ls keys 2>/dev/null", "r");
  #endif
  if (!p) return;
  char name[512];
  // Preferred provider selection: if set, try to find a key file that contains its name
  while (fgets(name, sizeof(name), p)) {
    size_t L = strlen(name); if (L && (name[L-1]=='\n' || name[L-1]=='\r')) name[L-1]='\0';
    char path[1024]; snprintf(path, sizeof(path), "keys/%s", name);
    // if preferred provider set, only accept files that match
    if (g_preferred_provider[0]) {
      if (!strcasestr_local(name, g_preferred_provider)) continue;
    }
    char* s = readFileToString(path);
    if (s) {
      char* p2 = s; while (*p2 && isspace((unsigned char)*p2)) p2++;
      char* q2 = p2 + strlen(p2) - 1; while (q2>p2 && isspace((unsigned char)*q2)) *q2--='\0';
      strncpy(g_api_key, p2, sizeof(g_api_key)-1);
      free(s);
      // break only if preferred provider set or if no preference we'll still try to find better match below
      if (g_preferred_provider[0]) break;
      // otherwise continue scanning to prefer known providers
    }
  }
  pclose(p);

  // If no preferred provider was set but we still haven't loaded a key, try prioritized names
  if (!g_api_key[0]) {
    const char* priorities[] = {"ollama", "qwen7b", "progassist", NULL};
    for (int i = 0; priorities[i]; ++i) {
      #ifdef _WIN32
      FILE* q = popen("dir /b keys 2>nul", "r");
      #else
      FILE* q = popen("ls keys 2>/dev/null", "r");
      #endif
      if (!q) continue;
      char name2[512];
      while (fgets(name2, sizeof(name2), q)) {
        size_t L2 = strlen(name2); if (L2 && (name2[L2-1]=='\n' || name2[L2-1]=='\r')) name2[L2-1]='\0';
        if (!strcasestr_local(name2, priorities[i])) continue;
        char path2[1024]; snprintf(path2, sizeof(path2), "keys/%s", name2);
        char* s2 = readFileToString(path2);
        if (s2) {
          char* p3 = s2; while (*p3 && isspace((unsigned char)*p3)) p3++;
          char* q3 = p3 + strlen(p3) - 1; while (q3>p3 && isspace((unsigned char)*q3)) *q3--='\0';
          strncpy(g_api_key, p3, sizeof(g_api_key)-1);
          free(s2);
          break;
        }
      }
      pclose(q);
      if (g_api_key[0]) break;
    }
  }
}

void set_preferred_provider(const char* prov) {
  if (!prov || !prov[0]) { g_preferred_provider[0]=0; return; }
  strncpy(g_preferred_provider, prov, sizeof(g_preferred_provider)-1);
  g_preferred_provider[sizeof(g_preferred_provider)-1]=0;
  // clear any previously loaded key so load_api_key will re-scan
  g_api_key[0]=0;
}

// Escape string for JSON (very small subset)
char* escape_json(const char* s) {
  size_t len = strlen(s);
  // worst case every char escapes -> 2x
  char* out = malloc(len * 2 + 1);
  if (!out) return NULL;
  char* d = out;
  for (const char* p = s; *p; ++p) {
    if (*p == '"') { *d++='\\'; *d++='"'; }
    else if (*p == '\\') { *d++='\\'; *d++='\\'; }
    else if (*p == '\n') { *d++='\\'; *d++='n'; }
    else *d++ = *p;
  }
  *d='\0';
  return out;
}

// Send a chat message. Supports Ollama (local CLI) when preferred provider is 'ollama', otherwise uses OpenAI-compatible API keys.
void cmd_chat(const char* prompt) {
  // If user selected Ollama as provider, call local ollama CLI (no key required)
  if (g_preferred_provider[0] && iequals(g_preferred_provider, "ollama")) {
    // Avoid auto-pulling large models. Require the model be installed locally.
    if (!ollama_model_installed(g_ollama_model)) {
      printf("Ollama model '%s' is not installed locally.\n", g_ollama_model);
      printf("To install a small model, pick one from `listmodels` or run: ollama pull <model>\n");
      printf("Set a different model with: setmodel <model>\n");
      return;
    }
    char* shell_esc = escape_shell_arg(prompt);
    if (!shell_esc) { printf("OOM\n"); return; }
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "ollama run %s \"%s\"", g_ollama_model, shell_esc);
    free(shell_esc);
    runCommandCapture(cmd);
    return;
  }

  load_api_key();
  if (!g_api_key[0]) { printf("No API key found in keys/. Place key file like keys/openai.key\n"); return; }
  char* esc = escape_json(prompt);
  if (!esc) { printf("Out of memory\n"); return; }
  // build JSON payload into a temp file to avoid command-line quoting issues
  const char* tmp = "__myaagent_payload.json";
  FILE* f = fopen(tmp, "w");
  if (!f) { free(esc); printf("Failed to write payload\n"); return; }
  fprintf(f, "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":800}", esc);
  fclose(f);
  free(esc);
  char cmd[2048];
  #ifdef _WIN32
  snprintf(cmd, sizeof(cmd), "curl -s -X POST https://api.openai.com/v1/chat/completions -H \"Content-Type: application/json\" -H \"Authorization: Bearer %s\" -d @%s", g_api_key, tmp);
  #else
  snprintf(cmd, sizeof(cmd), "curl -s -X POST https://api.openai.com/v1/chat/completions -H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @%s", g_api_key, tmp);
  #endif
  runCommandCapture(cmd);
  // remove tmp payload
  remove(tmp);
}

// Request code completion: provide file contents and position, print model response
void cmd_complete(const char* filename, const char* lineStr, const char* colStr) {
  // If using Ollama locally, call ollama CLI
  if (g_preferred_provider[0] && iequals(g_preferred_provider, "ollama")) {
    if (!ollama_model_installed(g_ollama_model)) {
      printf("Ollama model '%s' is not installed locally. Avoids auto-pulling large models.\n", g_ollama_model);
      printf("Install a small model with: ollama pull <model> or set a different model with: setmodel <model>\n");
      return;
    }
    int line = atoi(lineStr);
    int col = atoi(colStr);
    char* content = readFileToString(filename);
    if (!content) { printf("Could not open %s\n", filename); return; }
    char user_prompt[8192];
    snprintf(user_prompt, sizeof(user_prompt), "You are a helpful C programming assistant. Complete or suggest code changes around line %d column %d. Provide only the code patch or replacement, and keep context minimal.\n----\n%s\n----\nReply with the completed code snippet.", line, col, content);
    free(content);
    char* shell_esc = escape_shell_arg(user_prompt);
    if (!shell_esc) { printf("OOM\n"); return; }
    char cmd[12288];
    snprintf(cmd, sizeof(cmd), "ollama run %s \"%s\"", g_ollama_model, shell_esc);
    free(shell_esc);
    runCommandCapture(cmd);
    return;
  }
  load_api_key();
  if (!g_api_key[0]) { printf("No API key found in keys/. Place key file like keys/openai.key\n"); return; }
  int line = atoi(lineStr);
  int col = atoi(colStr);
  char* content = readFileToString(filename);
  if (!content) { printf("Could not open %s\n", filename); return; }
  // Build prompt
  char* esc_code = escape_json(content);
  free(content);
  if (!esc_code) { printf("OOM\n"); return; }
  char user_prompt[8192];
  snprintf(user_prompt, sizeof(user_prompt), "You are a helpful C programming assistant. Complete or suggest code changes around line %d column %d. Provide only the code patch or replacement, and keep context minimal.\n----\n%s\n----\nReply with the completed code snippet.", line, col, esc_code);
  free(esc_code);
  // Write payload
  const char* tmp = "__myaagent_payload.json";
  FILE* f = fopen(tmp, "w");
  if (!f) { printf("Failed to write payload\n"); return; }
  char* esc_prompt = escape_json(user_prompt);
  fprintf(f, "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":1200}", esc_prompt);
  free(esc_prompt);
  fclose(f);
  char cmd[4096];
  #ifdef _WIN32
  snprintf(cmd, sizeof(cmd), "curl -s -X POST https://api.openai.com/v1/chat/completions -H \"Content-Type: application/json\" -H \"Authorization: Bearer %s\" -d @%s", g_api_key, tmp);
  #else
  snprintf(cmd, sizeof(cmd), "curl -s -X POST https://api.openai.com/v1/chat/completions -H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @%s", g_api_key, tmp);
  #endif
  runCommandCapture(cmd);
  remove(tmp);
}

// Search simple files in current directory (non-recursive)
void cmd_search(const char* pattern) {
  // We'll scan a small set of extensions in the current directory
  const char* exts[] = {".c", ".h", ".txt"};
  for (int i = 0; i < 3; ++i) {
    const char* ext = exts[i];
    // naive scanning: open the directory and check each file name
    // Use popen to call dir listing for portability on Windows
    char cmd[512];
    #ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /b *%s", ext);
    #else
    snprintf(cmd, sizeof(cmd), "ls *%s 2>/dev/null", ext);
    #endif
    FILE* p = popen(cmd, "r");
    if (!p) continue;
    char fname[512];
    while (fgets(fname, sizeof(fname), p)) {
      // trim newline
      size_t L = strlen(fname);
      if (L && (fname[L-1]=='\n' || fname[L-1]=='\r')) fname[L-1] = '\0', --L;
      FILE* f = fopen(fname, "r");
      if (!f) continue;
      char buf[1024];
      int lineno = 0;
      while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        if (strstr(buf, pattern)) {
          printf("%s:%d: %s", fname, lineno, buf);
        }
      }
      fclose(f);
    }
    pclose(p);
  }
}

// Run a shell command and print output
void runCommandCapture(const char* cmd) {
  FILE* p = popen(cmd, "r");
  if (!p) {
    printf("Failed to run command: %s\n", cmd);
    return;
  }
  char buf[1024];
  while (fgets(buf, sizeof(buf), p)) fputs(buf, stdout);
  pclose(p);
}

// Show small templates
void cmd_template(const char* lang, const char* id) {
  if (strcmp(lang, "c") == 0) {
    if (strcmp(id, "hello") == 0) {
      printf("#include <stdio.h>\nint main(){printf(\"Hello, world\\n\");return 0;}\n");
    } else if (strcmp(id, "file") == 0) {
      printf("#include <stdio.h>\nint main(){FILE*f=fopen(\"input.txt\",\"r\");if(!f)return 1;int c;while((c=fgetc(f))!=EOF)putchar(c);fclose(f);return 0;}\n");
    } else {
      printf("Unknown template id for c. Try 'hello' or 'file'\n");
    }
  } else {
    printf("Unsupported language: %s\n", lang);
  }
}

int main() {
  char line[1024];
  printf("MyAIAgent — lightweight programmer assistant\n");
  printHelp();
  // Auto-detect Ollama and prefer it when available (local, no API key required)
  if (ollama_available()) {
    set_preferred_provider("ollama");
    // pick the smallest installed model when possible to avoid heavy downloads
    if (ollama_pick_smallest_model()) {
      printf("Ollama detected — defaulting provider to 'ollama' and using smallest local model '%s'.\n", g_ollama_model);
    } else {
      printf("Ollama detected on PATH — defaulting provider to 'ollama'. Use 'listmodels' or 'setmodel' to pick a model.\n");
    }
  } else {
    // Try to load any API key from keys/ so program can use remote providers if Ollama not present
    load_api_key();
    if (!g_api_key[0]) {
      printf("No API key found and Ollama not detected. To use a local model, install Ollama (https://ollama.ai) and run 'ollama pull <model>'.\n");
    }
  }
  while (true) {
    printf("\n> ");
    if (!fgets(line, sizeof(line), stdin)) break;
    // trim newline
    size_t L = strlen(line);
    if (L==0) continue;
    if (line[L-1]=='\n' || line[L-1]=='\r') line[L-1] = '\0';
    // parse command and arg
    char cmd[64];
    char arg1[512];
    char arg2[512];
    cmd[0]=arg1[0]=arg2[0]=0;
    // simple sscanf to split up to two args (rest ignored)
    int n = sscanf(line, "%63s %511s %511s", cmd, arg1, arg2);
    if (n <= 0) continue;
    if (strcmp(cmd, "help") == 0) {
      printHelp();
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
      break;
    } else if (strcmp(cmd, "open") == 0) {
      if (n < 2) { printf("Usage: open <file>\n"); continue; }
      cmd_open(arg1);
    } else if (strcmp(cmd, "chat") == 0) {
      if (n < 2) { printf("Usage: chat <message>\n"); continue; }
      // reconstruct rest of line after command to preserve spaces
      const char* start = strstr(line, " "); if (!start) { printf("Usage: chat <message>\n"); continue; }
      while (*start==' ') start++;
      cmd_chat(start);
    } else if (strcmp(cmd, "complete") == 0) {
      // parse the full command with three args: file, line, col
      char file[512], lineS[32], colS[32]; file[0]=lineS[0]=colS[0]=0;
      int m = sscanf(line, "%63s %511s %31s %31s", cmd, file, lineS, colS);
      if (m >= 4) {
        cmd_complete(file, lineS, colS);
      } else {
        printf("Usage: complete <file> <line> <col>\n");
      }
    } else if (strcmp(cmd, "search") == 0) {
      if (n < 2) { printf("Usage: search <pattern>\n"); continue; }
      cmd_search(arg1);
    } else if (strcmp(cmd, "usekey") == 0) {
      if (n < 2) { printf("Usage: usekey <provider>\n"); continue; }
      set_preferred_provider(arg1);
      load_api_key();
      if (g_api_key[0]) printf("Using key for provider '%s'\n", arg1);
      else if (iequals(arg1, "ollama")) printf("Using provider '%s' (local Ollama - no API key required)\n", arg1);
      else printf("No key found for provider '%s' in keys/\n", arg1);
    } else if (strcmp(cmd, "setkey") == 0) {
      if (n < 2) { printf("Usage: setkey <provider> <key>\n"); continue; }
      // reconstruct key (rest of line after provider)
      const char* startProv = strstr(line, arg1);
      const char* rest = "";
      if (startProv) {
        startProv += strlen(arg1);
        while (*startProv == ' ') startProv++;
        rest = startProv;
      }
      cmd_setkey(arg1, rest);
    } else if (strcmp(cmd, "setmodel") == 0) {
      if (n < 2) { printf("Usage: setmodel <model>\n"); continue; }
      strncpy(g_ollama_model, arg1, sizeof(g_ollama_model)-1);
      g_ollama_model[sizeof(g_ollama_model)-1] = '\0';
      printf("Ollama model set to '%s'\n", g_ollama_model);
    } else if (strcmp(cmd, "listmodels") == 0) {
      // list installed ollama models
      cmd_listmodels();
    } else if (strcmp(cmd, "compile") == 0) {
      if (n < 2) { printf("Usage: compile <source.c>\n"); continue; }
      char ccmd[1024];
      // compile to same name without extension
      char out[512];
      strncpy(out, arg1, sizeof(out)); out[sizeof(out)-1]=0;
      char* dot = strrchr(out, '.'); if (dot) *dot = '\0';
      #ifdef _WIN32
      snprintf(ccmd, sizeof(ccmd), "gcc %s -o %s.exe 2>&1", arg1, out);
      #else
      snprintf(ccmd, sizeof(ccmd), "gcc %s -o %s 2>&1", arg1, out);
      #endif
      runCommandCapture(ccmd);
    } else if (strcmp(cmd, "run") == 0) {
      if (n < 2) { printf("Usage: run <executable>\n"); continue; }
      char rcmd[1024];
      #ifdef _WIN32
      snprintf(rcmd, sizeof(rcmd), "%s", arg1);
      #else
      snprintf(rcmd, sizeof(rcmd), "./%s", arg1);
      #endif
      runCommandCapture(rcmd);
    } else if (strcmp(cmd, "template") == 0) {
      if (n < 3) { printf("Usage: template <language> <id>\n"); continue; }
      cmd_template(arg1, arg2);
    } else {
      printf("Unknown command: %s. Type 'help' for commands.\n", cmd);
    }
  }

  printf("Goodbye.\n");
  return 0;
}
