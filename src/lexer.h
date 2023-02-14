#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned char char16_t;
// extern unsigned char __heap_base;

const char16_t* STANDARD_IMPORT = (char16_t*)0x1;
const char16_t* IMPORT_META = (char16_t*)0x2;
const char16_t* REQUIRE = (char16_t*)0x3;
const char16_t __empty_char = '\0';
const char16_t* EMPTY_CHAR = &__empty_char;
// const char16_t* source = NULL;

// void setSource (void* ptr) {
//   source = ptr;
// }

struct Import {
  const char16_t* start;
  const char16_t* end;
  const char16_t* statement_start;
  const char16_t* statement_end;
  const char16_t* assert_index;
  const char16_t* dynamic;
  bool safe;
  struct Import* next;
};
typedef struct Import Import;

// Paren = odd, Brace = even
enum OpenTokenState {
  AnyParen = 1, // (
  AnyBrace = 2, // {
  Template = 3, // `
  TemplateBrace = 4, // ${
  ImportParen = 5, // import(),
  ClassBrace = 6,
  AsyncParen = 7, // async()
};

struct OpenToken {
  enum OpenTokenState token;
  char16_t* pos;
};
typedef struct OpenToken OpenToken;

struct Export {
  const char16_t* start;
  const char16_t* end;
  const char16_t* local_start;
  const char16_t* local_end;
  struct Export* next;
};
typedef struct Export Export;

typedef void *(*Allocator)(uint32_t bytes, void *user_data);

struct ParseResult {
  Import *first_import;
  Export *first_export;
  uint32_t parse_error;
};

typedef struct ParseResult ParseResult;

struct State {
  Allocator alloc;
  void *user_data;
  ParseResult *result;
  Import* import_write_head;
  Import* import_write_head_last;
  Export* export_write_head;
  bool facade;
  bool lastSlashWasDivision;
  uint16_t openTokenDepth;
  char16_t* lastTokenPos;
  char16_t *source;
  char16_t* pos;
  char16_t* end;
  OpenToken* openTokenStack;
  uint16_t dynamicImportStackDepth;
  Import** dynamicImportStack;
  bool nextBraceIsClass;
  bool has_error;
};

typedef struct State State;

// Memory Structure:
// -> source
// -> analysis starts after source
// uint32_t parse_error;
// bool has_error = false;
// uint32_t sourceLen = 0;

void bail (State *state, uint32_t err);

// allocateSource
// void sa (uint32_t utf16Len) {
  // sourceLen = utf16Len;
  // const char16_t* sourceEnd = source + utf16Len;
  // ensure source is null terminated
  // *(char16_t*)(source + utf16Len) = '\0';
  // analysis_base = (void*)sourceEnd;
  // analysis_base = malloc(10 * sizeof(Import));
  // analysis_head = analysis_base;
  // first_import = NULL;
  // import_write_head = NULL;
  // import_read_head = NULL;
  // first_export = NULL;
  // export_write_head = NULL;
  // export_read_head = NULL;
  // return source;
// }

void addImport (State *state, const char16_t* statement_start, const char16_t* start, const char16_t* end, const char16_t* dynamic) {
  // Import* import = (Import*)(analysis_head);
  // analysis_head = analysis_head + sizeof(Import);
  // Import *import = state->allocImport();
  Import *import = state->alloc(sizeof(Import), state->user_data);
  if (state->import_write_head == NULL)
    state->result->first_import = import;
  else
    state->import_write_head->next = import;
  state->import_write_head_last = state->import_write_head;
  state->import_write_head = import;
  import->statement_start = statement_start;
  if (dynamic == IMPORT_META)
    import->statement_end = end;
  else if (dynamic == STANDARD_IMPORT)
    import->statement_end = end + 1;
  else 
    import->statement_end = 0;
  import->start = start;
  import->end = end;
  import->assert_index = 0;
  import->dynamic = dynamic;
  import->safe = dynamic == STANDARD_IMPORT;
  import->next = NULL;
}

void addExport (State *state, const char16_t* start, const char16_t* end, const char16_t* local_start, const char16_t* local_end) {
  // Export* export = (Export*)(analysis_head);
  // analysis_head = analysis_head + sizeof(Export);
  // Export *export = state->allocExport();
  Export *export = state->alloc(sizeof(Export), state->user_data);
  if (state->export_write_head == NULL)
    state->result->first_export = export;
  else
    state->export_write_head->next = export;
  state->export_write_head = export;
  export->start = start;
  export->end = end;
  export->local_start = local_start;
  export->local_end = local_end;
  export->next = NULL;
}

// getErr
// uint32_t e () {
//   return parse_error;
// }

// // getImportStart
// uint32_t is () {
//   return import_read_head->start - source;
// }
// // getImportEnd
// uint32_t ie () {
//   return import_read_head->end == 0 ? -1 : import_read_head->end - source;
// }
// // getImportStatementStart
// uint32_t ss () {
//   return import_read_head->statement_start - source;
// }
// // getImportStatementEnd
// uint32_t se () {
//   return import_read_head->statement_end == 0 ? -1 : import_read_head->statement_end - source;
// }
// // getAssertIndex
// uint32_t ai () {
//   return import_read_head->assert_index == 0 ? -1 : import_read_head->assert_index - source;
// }
// // getImportDynamic
// uint32_t id () {
//   const char16_t* dynamic = import_read_head->dynamic;
//   if (dynamic == STANDARD_IMPORT)
//     return -1;
//   else if (dynamic == IMPORT_META)
//     return -2;
//   return import_read_head->dynamic - source;
// }
// // getImportSafeString
// uint32_t ip () {
//   return import_read_head->safe;
// }
// // getExportStart
// uint32_t es () {
//   return export_read_head->start - source;
// }
// // getExportEnd
// uint32_t ee () {
//   return export_read_head->end - source;
// }
// // getExportLocalStart
// int32_t els () {
//   return export_read_head->local_start ? export_read_head->local_start - source : -1;
// }
// // getExportLocalEnd
// int32_t ele () {
//   return export_read_head->local_end ? export_read_head->local_end - source : -1;
// }
// // readImport
// bool ri () {
//   printf("Testing %d\n", import_read_head);
//   if (import_read_head == NULL)
//     import_read_head = first_import;
//   else
//     import_read_head = import_read_head->next;
//   if (import_read_head == NULL)
//     return false;
//   return true;
// }
// // readExport
// bool re () {
//   if (export_read_head == NULL)
//     export_read_head = first_export;
//   else
//     export_read_head = export_read_head->next;
//   if (export_read_head == NULL)
//     return false;
//   return true;
// }
// bool f () {
//   return facade;
// }

bool parse ();

void tryParseImportStatement (State *state);
void tryParseExportStatement (State *state);
void tryParseRequire (State *state);

void readImportString (State *state, const char16_t* ss, char16_t ch);
char16_t readExportAs (State *state, char16_t* startPos, char16_t* endPos);

char16_t commentWhitespace (State *state, bool br);
void regularExpression (State *state);
void templateString (State *state);
void blockComment (State *state, bool br);
void lineComment (State *state);
void stringLiteral (State *state, char16_t quote);

char16_t readToWsOrPunctuator (State *state, char16_t ch);

bool isQuote (char16_t ch);

bool isBr (char16_t c);
bool isWsNotBr (char16_t c);
bool isBrOrWs (char16_t c);
bool isBrOrWsOrPunctuator (char16_t c);
bool isBrOrWsOrPunctuatorNotDot (char16_t c);

bool readPrecedingKeyword1(State *state, char16_t* pos, char16_t c1);
bool readPrecedingKeywordn(State *state, char16_t* pos, const char16_t* compare, size_t n);

bool isBreakOrContinue (State *state, char16_t* curPos);

bool keywordStart (State *state);
bool isExpressionKeyword (State *state, char16_t* pos);
bool isParenKeyword (State *state, char16_t* pos);
bool isPunctuator (char16_t charCode);
bool isExpressionPunctuator (char16_t charCode);
bool isExpressionTerminator (State *state, char16_t* pos);
bool isIdentifierChar(uint32_t code);
uint32_t nextChar(State *state);
static void *utf8_decode(void *buf, uint32_t *c, int *e);


void syntaxError (State *state);
