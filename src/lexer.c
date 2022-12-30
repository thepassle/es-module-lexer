#include "lexer.h"
#include <stdio.h>
#include <string.h>

// NOTE: MESSING WITH THESE REQUIRES MANUAL ASM DICTIONARY CONSTRUCTION (via lexer.emcc.js base64 decoding)
static const char16_t XPORT[] = { 'x', 'p', 'o', 'r', 't' };
static const char16_t MPORT[] = { 'm', 'p', 'o', 'r', 't' };
static const char16_t LASS[] = { 'l', 'a', 's', 's' };
static const char16_t FROM[] = { 'f', 'r', 'o', 'm' };
static const char16_t ETA[] = { 'e', 't', 'a' };
static const char16_t SSERT[] = { 's', 's', 'e', 'r', 't' };
static const char16_t VO[] = { 'v', 'o' };
static const char16_t YIE[] = { 'y', 'i', 'e' };
static const char16_t DELE[] = { 'd', 'e', 'l', 'e' };
static const char16_t INSTAN[] = { 'i', 'n', 's', 't', 'a', 'n' };
static const char16_t TY[] = { 't', 'y' };
static const char16_t RETUR[] = { 'r', 'e', 't', 'u', 'r' };
static const char16_t DEBUGGE[] = { 'd', 'e', 'b', 'u', 'g', 'g', 'e' };
static const char16_t AWAI[] = { 'a', 'w', 'a', 'i' };
static const char16_t THR[] = { 't', 'h', 'r' };
static const char16_t WHILE[] = { 'w', 'h', 'i', 'l', 'e' };
static const char16_t FOR[] = { 'f', 'o', 'r' };
static const char16_t IF[] = { 'i', 'f' };
static const char16_t CATC[] = { 'c', 'a', 't', 'c' };
static const char16_t FINALL[] = { 'f', 'i', 'n', 'a', 'l', 'l' };
static const char16_t ELS[] = { 'e', 'l', 's' };
static const char16_t BREA[] = { 'b', 'r', 'e', 'a' };
static const char16_t CONTIN[] = { 'c', 'o', 'n', 't', 'i', 'n' };
static const char16_t SYNC[] = {'s', 'y', 'n', 'c'};
static const char16_t UNCTION[] = {'u', 'n', 'c', 't', 'i', 'o', 'n'};

// Note: parsing is based on the _assumption_ that the source is already valid
bool parse (char16_t *source, uint32_t sourceLen, Allocator alloc, void *user_data, ParseResult *result) {
  // stack allocations
  // these are done here to avoid data section \0\0\0 repetition bloat
  // (while gzip fixes this, still better to have ~10KiB ungzipped over ~20KiB)
  OpenToken openTokenStack_[1024];
  Import* dynamicImportStack_[512];

  State state = {
    .facade = true,
    .dynamicImportStackDepth = 0,
    .openTokenDepth = 0,
    .lastTokenPos = (char16_t*)EMPTY_CHAR,
    .lastSlashWasDivision = false,
    .has_error = false,
    .openTokenStack = &openTokenStack_[0],
    .dynamicImportStack = &dynamicImportStack_[0],
    .nextBraceIsClass = false,
    .source = source,
    .alloc = alloc,
    .user_data = user_data,
    .result = result,
  };

  state.pos = (char16_t*)(source - 1);
  char16_t ch = '\0';
  state.end = state.pos + sourceLen;

  // start with a pure "module-only" parser
  while (state.pos++ < state.end) {
    ch = *state.pos;

    if (ch == 32 || ch < 14 && ch > 8)
      continue;

    switch (ch) {
      case 'e':
        if (state.openTokenDepth == 0 && keywordStart(&state) && memcmp(state.pos + 1, &XPORT[0], 5 * sizeof(char16_t)) == 0) {
          tryParseExportStatement(&state);
          // export might have been a non-pure declaration
          if (!state.facade) {
            state.lastTokenPos = state.pos;
            goto mainparse;
          }
        }
        break;
      case 'i':
        if (keywordStart(&state) && memcmp(state.pos + 1, &MPORT[0], 5 * sizeof(char16_t)) == 0)
          tryParseImportStatement(&state);
        break;
      case ';':
        break;
      case '/': {
        char16_t next_ch = *(state.pos + 1);
        if (next_ch == '/') {
          lineComment(&state);
          // dont update lastToken
          continue;
        }
        else if (next_ch == '*') {
          blockComment(&state, true);
          // dont update lastToken
          continue;
        }
        // fallthrough
      }
      default:
        // as soon as we hit a non-module token, we go to main parser
        state.facade = false;
        state.pos--;
        goto mainparse; // oh yeahhh
    }
    state.lastTokenPos = state.pos;
  }

  if (state.has_error)
    return false;

  mainparse: while (state.pos++ < state.end) {
    ch = *state.pos;

    if (ch == 32 || ch < 14 && ch > 8)
      continue;

    switch (ch) {
      case 'e':
        if (state.openTokenDepth == 0 && keywordStart(&state) && memcmp(state.pos + 1, &XPORT[0], 5 * sizeof(char16_t)) == 0)
          tryParseExportStatement(&state);
        break;
      case 'i':
        if (keywordStart(&state) && memcmp(state.pos + 1, &MPORT[0], 5 * sizeof(char16_t)) == 0)
          tryParseImportStatement(&state);
        break;
      case 'c':
        if (keywordStart(&state) && memcmp(state.pos + 1, &LASS[0], 4 * sizeof(char16_t)) == 0 && isBrOrWs(*(state.pos + 5)))
          state.nextBraceIsClass = true;
        break;
      case '(':
        state.openTokenStack[state.openTokenDepth].token = AnyParen;
        state.openTokenStack[state.openTokenDepth++].pos = state.lastTokenPos;
        break;
      case ')':
        if (state.openTokenDepth == 0)
          return syntaxError(&state), false;
        state.openTokenDepth--;
        if (state.dynamicImportStackDepth > 0 && state.dynamicImportStack[state.dynamicImportStackDepth - 1]->dynamic == state.openTokenStack[state.openTokenDepth].pos) {
          Import* cur_dynamic_import = state.dynamicImportStack[state.dynamicImportStackDepth - 1];
          if (cur_dynamic_import->end == 0)
            cur_dynamic_import->end = state.pos;
          cur_dynamic_import->statement_end = state.pos + 1;
          state.dynamicImportStackDepth--;
        }
        break;
      case '{':
        // dynamic import followed by { is not a dynamic import (so remove)
        // this is a sneaky way to get around { import () {} } v { import () }
        // block / object ambiguity without a parser (assuming source is valid)
        if (*state.lastTokenPos == ')' && state.import_write_head && state.import_write_head->end == state.lastTokenPos) {
          state.import_write_head = state.import_write_head_last;
          if (state.import_write_head)
            state.import_write_head->next = NULL;
          else
            state.result->first_import = NULL;
        }
        state.openTokenStack[state.openTokenDepth].token = state.nextBraceIsClass ? ClassBrace : AnyBrace;
        state.openTokenStack[state.openTokenDepth++].pos = state.lastTokenPos;
        state.nextBraceIsClass = false;
        break;
      case '}':
        if (state.openTokenDepth == 0)
          return syntaxError(&state), false;
        if (state.openTokenStack[--state.openTokenDepth].token == TemplateBrace) {
          templateString(&state);
        }
        break;
      case '\'':
        stringLiteral(&state, ch);
        break;
      case '"':
        stringLiteral(&state, ch);
        break;
      case '/': {
        char16_t next_ch = *(state.pos + 1);
        if (next_ch == '/') {
          lineComment(&state);
          // dont update lastToken
          continue;
        }
        else if (next_ch == '*') {
          blockComment(&state, true);
          // dont update lastToken
          continue;
        }
        else {
          // Division / regex ambiguity handling based on checking backtrack analysis of:
          // - what token came previously (lastToken)
          // - if a closing brace or paren, what token came before the corresponding
          //   opening brace or paren (lastOpenTokenIndex)
          char16_t lastToken = *state.lastTokenPos;
          if (isExpressionPunctuator(lastToken) &&
              !(lastToken == '.' && (*(state.lastTokenPos - 1) >= '0' && *(state.lastTokenPos - 1) <= '9')) &&
              !(lastToken == '+' && *(state.lastTokenPos - 1) == '+') && !(lastToken == '-' && *(state.lastTokenPos - 1) == '-') ||
              lastToken == ')' && isParenKeyword(&state, state.openTokenStack[state.openTokenDepth].pos) ||
              lastToken == '}' && (isExpressionTerminator(&state, state.openTokenStack[state.openTokenDepth].pos) || state.openTokenStack[state.openTokenDepth].token == ClassBrace) ||
              isExpressionKeyword(&state, state.lastTokenPos) ||
              lastToken == '/' && state.lastSlashWasDivision ||
              !lastToken) {
            regularExpression(&state);
            state.lastSlashWasDivision = false;
          }
          else {
            // Final check - if the last token was "break x" or "continue x"
            while (state.lastTokenPos > source && !isBrOrWsOrPunctuatorNotDot(*(--state.lastTokenPos)));
            if (isWsNotBr(*state.lastTokenPos)) {
              while (state.lastTokenPos > source && isWsNotBr(*(--state.lastTokenPos)));
              if (isBreakOrContinue(&state, state.lastTokenPos)) {
                regularExpression(&state);
                state.lastSlashWasDivision = false;
                break;
              }
            }
            state.lastSlashWasDivision = true;
          }
        }
        break;
      }
      case '`':
        state.openTokenStack[state.openTokenDepth].pos = state.lastTokenPos;
        state.openTokenStack[state.openTokenDepth++].token = Template;
        templateString(&state);
        break;
    }
    state.lastTokenPos = state.pos;
  }

  if (state.openTokenDepth || state.has_error || state.dynamicImportStackDepth)
    return false;

  // succeess
  return true;
}

void tryParseImportStatement (State *state) {
  char16_t* startPos = state->pos;

  state->pos += 6;

  char16_t ch = commentWhitespace(state, true);

  switch (ch) {
    // dynamic import
    case '(':
      state->openTokenStack[state->openTokenDepth].token = ImportParen;
      state->openTokenStack[state->openTokenDepth++].pos = state->pos;
      if (*state->lastTokenPos == '.')
        return;
      // dynamic import indicated by positive d
      char16_t* dynamicPos = state->pos;
      // try parse a string, to record a safe dynamic import string
      state->pos++;
      ch = commentWhitespace(state, true);
      addImport(state, startPos, state->pos, 0, dynamicPos);
      state->dynamicImportStack[state->dynamicImportStackDepth++] = state->import_write_head;
      if (ch == '\'') {
        stringLiteral(state, ch);
      }
      else if (ch == '"') {
        stringLiteral(state, ch);
      }
      else {
        state->pos--;
        return;
      }
      state->pos++;
      char16_t* endPos = state->pos;
      ch = commentWhitespace(state, true);
      if (ch == ',') {
        state->pos++;
        ch = commentWhitespace(state, true);
        state->import_write_head->end = endPos;
        state->import_write_head->assert_index = state->pos;
        state->import_write_head->safe = true;
        state->pos--;
      }
      else if (ch == ')') {
        state->openTokenDepth--;
        state->import_write_head->end = endPos;
        state->import_write_head->statement_end = state->pos + 1;
        state->import_write_head->safe = true;
        state->dynamicImportStackDepth--;
      }
      else {
        state->pos--;
      }
      return;
    // import.meta
    case '.':
      state->pos++;
      ch = commentWhitespace(state, true);
      // import.meta indicated by d == -2
      if (ch == 'm' && memcmp(state->pos + 1, &ETA[0], 3 * sizeof(char16_t)) == 0 && *state->lastTokenPos != '.')
        addImport(state, startPos, startPos, state->pos + 4, IMPORT_META);
      return;

    default:
      // no space after "import" -> not an import keyword
      if (state->pos == startPos + 6) {
        state->pos--;
        break;
      }
    case '"':
    case '\'':
    case '*': {
      // import statement only permitted at base-level
      if (state->openTokenDepth != 0) {
        state->pos--;
        return;
      }
      while (state->pos < state->end) {
        ch = *state->pos;
        if (isQuote(ch)) {
          readImportString(state, startPos, ch);
          return;
        }
        state->pos++;
      }
      syntaxError(state);
      break;
    }

    case '{': {
      // import statement only permitted at base-level
      if (state->openTokenDepth != 0) {
        state->pos--;
        return;
      }

      while (state->pos < state->end) {
        ch = commentWhitespace(state, true);

        if (isQuote(ch)) {
          stringLiteral(state, ch);
        } else if (ch == '}') {
          state->pos++;
          break;
        }

        state->pos++;
      }

      ch = commentWhitespace(state, true);
      if (memcmp(state->pos, &FROM[0], 4 * sizeof(char16_t)) != 0) {
        syntaxError(state);
        break;
      }

      state->pos += 4;
      ch = commentWhitespace(state, true);

      if (!isQuote(ch)) {
        return syntaxError(state);
      }

      readImportString(state, startPos, ch);

      break;
    }
  }
}

void tryParseExportStatement (State *state) {
  char16_t* sStartPos = state->pos;
  Export* prev_export_write_head = state->export_write_head;

  state->pos += 6;

  char16_t* curPos = state->pos;

  char16_t ch = commentWhitespace(state, true);

  if (state->pos == curPos && !isPunctuator(ch))
    return;

  if (ch == '{') {
    state->pos++;
    ch = commentWhitespace(state, true);
    while (true) {
      char16_t* startPos = state->pos;

      if (!isQuote(ch)) {
        ch = readToWsOrPunctuator(state, ch);
      }
      // export { "identifer" as } from
      // export { "@notid" as } from
      // export { "spa ce" as } from
      // export { " space" as } from
      // export { "space " as } from
      // export { "not~id" as } from
      // export { "%notid" as } from
      // export { "identifer" } from
      // export { "%notid" } from
      else {
        stringLiteral(state, ch);
        state->pos++;
      }

      char16_t* endPos = state->pos;
      commentWhitespace(state, true);
      ch = readExportAs(state, startPos, endPos);
      // ,
      if (ch == ',') {
        state->pos++;
        ch = commentWhitespace(state, true);
      }
      if (ch == '}')
        break;
      if (state->pos == startPos)
        return syntaxError(state);
      if (state->pos > state->end)
        return syntaxError(state);
    }
    state->pos++;
    ch = commentWhitespace(state, true);
  }
  // export *
  // export * as X
  else if (ch == '*') {
    state->pos++;
    commentWhitespace(state, true);
    ch = readExportAs(state, state->pos, state->pos);
    ch = commentWhitespace(state, true);
  }
  else {
    state->facade = false;
    switch (ch) {
      // export default ...
      case 'd': {
        const char16_t* startPos = state->pos;
        state->pos += 7;
        ch = commentWhitespace(state, true);
        bool localName = false;
        // export default async? function*? name? (){}
        if (ch == 'a' && keywordStart(state) &&  memcmp(state->pos + 1, &SYNC[0], 4 * sizeof(char16_t)) == 0 && isWsNotBr(*(state->pos + 5))) {
          state->pos += 5;
          ch = commentWhitespace(state, false);
        }
        if (ch == 'f' && keywordStart(state) && memcmp(state->pos + 1, &UNCTION[0], 7 * sizeof(char16_t)) == 0 && (isBrOrWs(*(state->pos + 8)) || *(state->pos + 8) == '*' || *(state->pos + 8) == '(')) {
          state->pos += 8;
          ch = commentWhitespace(state, true);
          if (ch == '*') {
            state->pos++;
            ch = commentWhitespace(state, true);
          }
          if (ch == '(') {
            addExport(state, startPos, startPos + 7, NULL, NULL);
            state->pos = (char16_t*)(startPos + 6);
            return;
          }
          localName = true;
        }
        // export default class name? {}
        if (ch == 'c' && keywordStart(state) && memcmp(state->pos + 1, &LASS[0], 4 * sizeof(char16_t)) == 0 && (isBrOrWs(*(state->pos + 5)) || *(state->pos + 5) == '{')) {
          state->pos += 5;
          ch = commentWhitespace(state, true);
          if (ch == '{') {
            addExport(state, startPos, startPos + 7, NULL, NULL);
            state->pos = (char16_t*)(startPos + 6);
            return;
          }
          localName = true;
        }
        const char16_t* localStartPos = state->pos;
        ch = readToWsOrPunctuator(state, ch);
        if (localName && state->pos > localStartPos) {
          addExport(state, startPos, startPos + 7, localStartPos, state->pos);
          state->pos--;
        }
        else {
          addExport(state, startPos, startPos + 7, NULL, NULL);
          state->pos = (char16_t*)(startPos + 6);
        }
        return;
      }
      // export async? function*? name () {
      case 'a':
        state->pos += 5;
        commentWhitespace(state, true);
      // fallthrough
      case 'f':
        state->pos += 8;
        ch = commentWhitespace(state, true);
        if (ch == '*') {
          state->pos++;
          ch = commentWhitespace(state, true);
        }
        const char16_t* startPos = state->pos;
        ch = readToWsOrPunctuator(state, ch);
        addExport(state, startPos, state->pos, startPos, state->pos);
        state->pos--;
        return;

      // export class name ...
      case 'c':
        if (memcmp(state->pos + 1, &LASS[0], 4 * sizeof(char16_t)) == 0 && isBrOrWsOrPunctuatorNotDot(*(state->pos + 5))) {
          state->pos += 5;
          ch = commentWhitespace(state, true);
          const char16_t* startPos = state->pos;
          ch = readToWsOrPunctuator(state, ch);
          addExport(state, startPos, state->pos, startPos, state->pos);
          state->pos--;
          return;
        }
        state->pos += 2;
      // fallthrough

      // export var/let/const name = ...(, name = ...)+
      case 'v':
      case 'l':
        // destructured initializations not currently supported (skipped for { or [)
        // also, lexing names after variable equals is skipped (export var p = function () { ... }, q = 5 skips "q")
        state->pos += 2;
        state->facade = false;
        do {
          state->pos++;
          ch = commentWhitespace(state, true);
          const char16_t* startPos = state->pos;
          ch = readToWsOrPunctuator(state, ch);
          // dont yet handle [ { destructurings
          if (ch == '{' || ch == '[') {
            break;
          }
          if (state->pos == startPos)
            return;
          addExport(state, startPos, state->pos, startPos, state->pos);
          ch = commentWhitespace(state, true);
          if (ch == '=') {
            break;
          }
        } while (ch == ',');
        state->pos--;
        return;

      default:
        return;
    }
  }

  // from ...
  if (ch == 'f' && memcmp(state->pos + 1, &FROM[1], 3 * sizeof(char16_t)) == 0) {
    state->pos += 4;
    readImportString(state, sStartPos, commentWhitespace(state, true));

    // There were no local names.
    for (Export* exprt = prev_export_write_head == NULL ? state->result->first_export : prev_export_write_head->next; exprt != NULL; exprt = exprt->next) {
      exprt->local_start = exprt->local_end = NULL;
    }
  }
  else {
    state->pos--;
  }
}

char16_t readExportAs (State *state, char16_t* startPos, char16_t* endPos) {
  char16_t ch = *state->pos;
  char16_t* localStartPos = startPos == endPos ? NULL : startPos;
  char16_t* localEndPos = startPos == endPos ? NULL : endPos;

  if (ch == 'a') {
    state->pos += 2;
    ch = commentWhitespace(state, true);
    startPos = state->pos;

    if (!isQuote(ch)) {
      ch = readToWsOrPunctuator(state, ch);
    }
    // export { mod as "identifer" } from
    // export { mod as "@notid" } from
    // export { mod as "spa ce" } from
    // export { mod as " space" } from
    // export { mod as "space " } from
    // export { mod as "not~id" } from
    // export { mod as "%notid" } from
    else {
      stringLiteral(state, ch);
      state->pos++;
    }

    endPos = state->pos;

    ch = commentWhitespace(state, true);
  }

  if (state->pos != startPos)
    addExport(state, startPos, endPos, localStartPos, localEndPos);
  return ch;
}

void readImportString (State *state, const char16_t* ss, char16_t ch) {
  const char16_t* startPos = state->pos + 1;
  if (ch == '\'') {
    stringLiteral(state, ch);
  }
  else if (ch == '"') {
    stringLiteral(state, ch);
  }
  else {
    syntaxError(state);
    return;
  }
  addImport(state, ss, startPos, state->pos, STANDARD_IMPORT);
  state->pos++;
  ch = commentWhitespace(state, false);
  if (ch != 'a' || memcmp(state->pos + 1, &SSERT[0], 5 * sizeof(char16_t)) != 0) {
    state->pos--;
    return;
  }
  char16_t* assertIndex = state->pos;
  state->pos += 6;
  ch = commentWhitespace(state, true);
  if (ch != '{') {
    state->pos = assertIndex;
    return;
  }
  const char16_t* assertStart = state->pos;
  do {
    state->pos++;
    ch = commentWhitespace(state, true);
    if (ch == '\'') {
      stringLiteral(state, ch);
      state->pos++;
      ch = commentWhitespace(state, true);
    }
    else if (ch == '"') {
      stringLiteral(state, ch);
      state->pos++;
      ch = commentWhitespace(state, true);
    }
    else {
      ch = readToWsOrPunctuator(state, ch);
    }
    if (ch != ':') {
      state->pos = assertIndex;
      return;
    }
    state->pos++;
    ch = commentWhitespace(state, true);
    if (ch == '\'') {
      stringLiteral(state, ch);
    }
    else if (ch == '"') {
      stringLiteral(state, ch);
    }
    else {
      state->pos = assertIndex;
      return;
    }
    state->pos++;
    ch = commentWhitespace(state, true);
    if (ch == ',') {
      state->pos++;
      ch = commentWhitespace(state, true);
      if (ch == '}')
        break;
      continue;
    }
    if (ch == '}')
      break;
    state->pos = assertIndex;
    return;
  } while (true);
  state->import_write_head->assert_index = assertStart;
  state->import_write_head->statement_end = state->pos + 1;
}

char16_t commentWhitespace (State *state, bool br) {
  char16_t ch;
  do {
    ch = *state->pos;
    if (ch == '/') {
      char16_t next_ch = *(state->pos + 1);
      if (next_ch == '/')
        lineComment(state);
      else if (next_ch == '*')
        blockComment(state, br);
      else
        return ch;
    }
    else if (br ? !isBrOrWs(ch) : !isWsNotBr(ch)) {
      return ch;
    }
  } while (state->pos++ < state->end);
  return ch;
}

void templateString (State *state) {
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (ch == '$' && *(state->pos + 1) == '{') {
      state->pos++;
      state->openTokenStack[state->openTokenDepth].token = TemplateBrace;
      state->openTokenStack[state->openTokenDepth++].pos = state->pos;
      return;
    }
    if (ch == '`') {
      if (state->openTokenStack[--state->openTokenDepth].token != Template)
        syntaxError(state);
      return;
    }
    if (ch == '\\')
      state->pos++;
  }
  syntaxError(state);
}

void blockComment (State *state, bool br) {
  state->pos++;
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (!br && isBr(ch))
      return;
    if (ch == '*' && *(state->pos + 1) == '/') {
      state->pos++;
      return;
    }
  }
}

void lineComment (State *state) {
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (ch == '\n' || ch == '\r')
      return;
  }
}

void stringLiteral (State *state, char16_t quote) {
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (ch == quote)
      return;
    if (ch == '\\') {
      ch = *++state->pos;
      if (ch == '\r' && *(state->pos + 1) == '\n')
        state->pos++;
    }
    else if (isBr(ch))
      break;
  }
  syntaxError(state);
}

char16_t regexCharacterClass (State *state) {
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (ch == ']')
      return ch;
    if (ch == '\\')
      state->pos++;
    else if (ch == '\n' || ch == '\r')
      break;
  }
  syntaxError(state);
  return '\0';
}

void regularExpression (State *state) {
  while (state->pos++ < state->end) {
    char16_t ch = *state->pos;
    if (ch == '/')
      return;
    if (ch == '[')
      ch = regexCharacterClass(state);
    else if (ch == '\\')
      state->pos++;
    else if (ch == '\n' || ch == '\r')
      break;
  }
  syntaxError(state);
}

char16_t readToWsOrPunctuator (State *state, char16_t ch) {
  do {
    if (isBrOrWs(ch) || isPunctuator(ch))
      return ch;
  } while (ch = *(++state->pos));
  return ch;
}

// Note: non-asii BR and whitespace checks omitted for perf / footprint
// if there is a significant user need this can be reconsidered
bool isBr (char16_t c) {
  return c == '\r' || c == '\n';
}

bool isWsNotBr (char16_t c) {
  return c == 9 || c == 11 || c == 12 || c == 32 || c == 160;
}

bool isBrOrWs (char16_t c) {
  return c > 8 && c < 14 || c == 32 || c == 160;
}

bool isBrOrWsOrPunctuatorNotDot (char16_t c) {
  return c > 8 && c < 14 || c == 32 || c == 160 || isPunctuator(c) && c != '.';
}

bool isQuote (char16_t ch) {
  return ch == '\'' || ch == '"';
}

bool keywordStart (State *state) {
  return state->pos == state->source || isBrOrWsOrPunctuatorNotDot(*(state->pos - 1));
}

bool readPrecedingKeyword1 (State *state, char16_t* pos, char16_t c1) {
  if (pos < state->source) return false;
  return *pos == c1 && (pos == state->source || isBrOrWsOrPunctuatorNotDot(*(pos - 1)));
}

bool readPrecedingKeywordn (State *state, char16_t* pos, const char16_t* compare, size_t n) {
  if (pos - n + 1 < state->source) return false;
  return memcmp(pos - n + 1, compare, n * sizeof(char16_t)) == 0 && (pos - n + 1 == state->source || isBrOrWsOrPunctuatorNotDot(*(pos - n)));
}

// Detects one of case, debugger, delete, do, else, in, instanceof, new,
//   return, throw, typeof, void, yield ,await
bool isExpressionKeyword (State *state, char16_t* pos) {
  switch (*pos) {
    case 'd':
      switch (*(pos - 1)) {
        case 'i':
          // void
          return readPrecedingKeywordn(state, pos - 2, &VO[0], 2);
        case 'l':
          // yield
          return readPrecedingKeywordn(state, pos - 2, &YIE[0], 3);
        default:
          return false;
      }
    case 'e':
      switch (*(pos - 1)) {
        case 's':
          switch (*(pos - 2)) {
            case 'l':
              // else
              return readPrecedingKeyword1(state, pos - 3, 'e');
            case 'a':
              // case
              return readPrecedingKeyword1(state, pos - 3, 'c');
            default:
              return false;
          }
        case 't':
          // delete
          return readPrecedingKeywordn(state, pos - 2, &DELE[0], 4);
        case 'u':
          // continue
          return readPrecedingKeywordn(state, pos - 2, &CONTIN[0], 6);
        default:
          return false;
      }
    case 'f':
      if (*(pos - 1) != 'o' || *(pos - 2) != 'e')
        return false;
      switch (*(pos - 3)) {
        case 'c':
          // instanceof
          return readPrecedingKeywordn(state, pos - 4, &INSTAN[0], 6);
        case 'p':
          // typeof
          return readPrecedingKeywordn(state, pos - 4, &TY[0], 2);
        default:
          return false;
      }
    case 'k':
      // break
      return readPrecedingKeywordn(state, pos - 1, &BREA[0], 4);
    case 'n':
      // in, return
      return readPrecedingKeyword1(state, pos - 1, 'i') || readPrecedingKeywordn(state, pos - 1, &RETUR[0], 5);
    case 'o':
      // do
      return readPrecedingKeyword1(state, pos - 1, 'd');
    case 'r':
      // debugger
      return readPrecedingKeywordn(state, pos - 1, &DEBUGGE[0], 7);
    case 't':
      // await
      return readPrecedingKeywordn(state, pos - 1, &AWAI[0], 4);
    case 'w':
      switch (*(pos - 1)) {
        case 'e':
          // new
          return readPrecedingKeyword1(state, pos - 2, 'n');
        case 'o':
          // throw
          return readPrecedingKeywordn(state, pos - 2, &THR[0], 3);
        default:
          return false;
      }
  }
  return false;
}

bool isParenKeyword (State *state, char16_t* curPos) {
  return readPrecedingKeywordn(state, curPos, &WHILE[0], 5) ||
      readPrecedingKeywordn(state, curPos, &FOR[0], 3) ||
      readPrecedingKeywordn(state, curPos, &IF[0], 2);
}

bool isPunctuator (char16_t ch) {
  // 23 possible punctuator endings: !%&()*+,-./:;<=>?[]^{}|~
  return ch == '!' || ch == '%' || ch == '&' ||
    ch > 39 && ch < 48 || ch > 57 && ch < 64 ||
    ch == '[' || ch == ']' || ch == '^' ||
    ch > 122 && ch < 127;
}

bool isExpressionPunctuator (char16_t ch) {
  // 20 possible expression endings: !%&(*+,-.:;<=>?[^{|~
  return ch == '!' || ch == '%' || ch == '&' ||
    ch > 39 && ch < 47 && ch != 41 || ch > 57 && ch < 64 ||
    ch == '[' || ch == '^' || ch > 122 && ch < 127 && ch != '}';
}

bool isBreakOrContinue (State *state, char16_t* curPos) {
  switch (*curPos) {
    case 'k':
      return readPrecedingKeywordn(state, curPos - 1, &BREA[0], 4);
    case 'e':
      if (*(curPos - 1) == 'u')
        return readPrecedingKeywordn(state, curPos - 2, &CONTIN[0], 6);
  }
  return false;
}

bool isExpressionTerminator (State *state, char16_t* curPos) {
  // detects:
  // => ; ) finally catch else class X
  // as all of these followed by a { will indicate a statement brace
  switch (*curPos) {
    case '>':
      return *(curPos - 1) == '=';
    case ';':
    case ')':
      return true;
    case 'h':
      return readPrecedingKeywordn(state, curPos - 1, &CATC[0], 4);
    case 'y':
      return readPrecedingKeywordn(state, curPos - 1, &FINALL[0], 6);
    case 'e':
      return readPrecedingKeywordn(state, curPos - 1, &ELS[0], 3);
  }
  return false;
}

void bail (State *state, uint32_t error) {
  state->has_error = true;
  state->result->parse_error = error;
  state->pos = state->end + 1;
}

void syntaxError (State *state) {
  state->has_error = true;
  state->result->parse_error = state->pos - state->source;
  state->pos = state->end + 1;
}
