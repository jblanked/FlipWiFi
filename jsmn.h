/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

  /**
   * JSON type identifier. Basic types are:
   * 	o Object
   * 	o Array
   * 	o String
   * 	o Other primitive: number, boolean (true/false) or null
   */
  typedef enum
  {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1 << 0,
    JSMN_ARRAY = 1 << 1,
    JSMN_STRING = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3
  } jsmntype_t;

  enum jsmnerr
  {
    /* Not enough tokens were provided */
    JSMN_ERROR_NOMEM = -1,
    /* Invalid character inside JSON string */
    JSMN_ERROR_INVAL = -2,
    /* The string is not a full JSON packet, more bytes expected */
    JSMN_ERROR_PART = -3
  };

  /**
   * JSON token description.
   * type		type (object, array, string etc.)
   * start	start position in JSON data string
   * end		end position in JSON data string
   */
  typedef struct jsmntok
  {
    jsmntype_t type;
    int start;
    int end;
    int size;
#ifdef JSMN_PARENT_LINKS
    int parent;
#endif
  } jsmntok_t;

  /**
   * JSON parser. Contains an array of token blocks available. Also stores
   * the string being parsed now and current position in that string.
   */
  typedef struct jsmn_parser
  {
    unsigned int pos;     /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper;         /* superior token node, e.g. parent object or array */
  } jsmn_parser;

  /**
   * Create JSON parser over an array of tokens
   */
  JSMN_API void jsmn_init(jsmn_parser *parser);

  /**
   * Run JSON parser. It parses a JSON data string into and array of tokens, each
   * describing
   * a single JSON object.
   */
  JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                          jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
  /**
   * Allocates a fresh unused token from the token pool.
   */
  static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                     const size_t num_tokens)
  {
    jsmntok_t *tok;
    if (parser->toknext >= num_tokens)
    {
      return NULL;
    }
    tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
  }

  /**
   * Fills token type and boundaries.
   */
  static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                              const int start, const int end)
  {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
  }

  /**
   * Fills next available token with JSON primitive.
   */
  static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                  const size_t len, jsmntok_t *tokens,
                                  const size_t num_tokens)
  {
    jsmntok_t *token;
    int start;

    start = parser->pos;

    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++)
    {
      switch (js[parser->pos])
      {
#ifndef JSMN_STRICT
      /* In strict mode primitive must be followed by "," or "}" or "]" */
      case ':':
#endif
      case '\t':
      case '\r':
      case '\n':
      case ' ':
      case ',':
      case ']':
      case '}':
        goto found;
      default:
        /* to quiet a warning from gcc*/
        break;
      }
      if (js[parser->pos] < 32 || js[parser->pos] >= 127)
      {
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
#ifdef JSMN_STRICT
    /* In strict mode primitive must be followed by a comma/object/array */
    parser->pos = start;
    return JSMN_ERROR_PART;
#endif

  found:
    if (tokens == NULL)
    {
      parser->pos--;
      return 0;
    }
    token = jsmn_alloc_token(parser, tokens, num_tokens);
    if (token == NULL)
    {
      parser->pos = start;
      return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
  }

  /**
   * Fills next token with JSON string.
   */
  static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                               const size_t len, jsmntok_t *tokens,
                               const size_t num_tokens)
  {
    jsmntok_t *token;

    int start = parser->pos;

    /* Skip starting quote */
    parser->pos++;

    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++)
    {
      char c = js[parser->pos];

      /* Quote: end of string */
      if (c == '\"')
      {
        if (tokens == NULL)
        {
          return 0;
        }
        token = jsmn_alloc_token(parser, tokens, num_tokens);
        if (token == NULL)
        {
          parser->pos = start;
          return JSMN_ERROR_NOMEM;
        }
        jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
        token->parent = parser->toksuper;
#endif
        return 0;
      }

      /* Backslash: Quoted symbol expected */
      if (c == '\\' && parser->pos + 1 < len)
      {
        int i;
        parser->pos++;
        switch (js[parser->pos])
        {
        /* Allowed escaped symbols */
        case '\"':
        case '/':
        case '\\':
        case 'b':
        case 'f':
        case 'r':
        case 'n':
        case 't':
          break;
        /* Allows escaped symbol \uXXXX */
        case 'u':
          parser->pos++;
          for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0';
               i++)
          {
            /* If it isn't a hex character we have an error */
            if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
                  (js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
                  (js[parser->pos] >= 97 && js[parser->pos] <= 102)))
            { /* a-f */
              parser->pos = start;
              return JSMN_ERROR_INVAL;
            }
            parser->pos++;
          }
          parser->pos--;
          break;
        /* Unexpected symbol */
        default:
          parser->pos = start;
          return JSMN_ERROR_INVAL;
        }
      }
    }
    parser->pos = start;
    return JSMN_ERROR_PART;
  }

  /**
   * Parse JSON string and fill tokens.
   */
  JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                          jsmntok_t *tokens, const unsigned int num_tokens)
  {
    int r;
    int i;
    jsmntok_t *token;
    int count = parser->toknext;

    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++)
    {
      char c;
      jsmntype_t type;

      c = js[parser->pos];
      switch (c)
      {
      case '{':
      case '[':
        count++;
        if (tokens == NULL)
        {
          break;
        }
        token = jsmn_alloc_token(parser, tokens, num_tokens);
        if (token == NULL)
        {
          return JSMN_ERROR_NOMEM;
        }
        if (parser->toksuper != -1)
        {
          jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
          /* In strict mode an object or array can't become a key */
          if (t->type == JSMN_OBJECT)
          {
            return JSMN_ERROR_INVAL;
          }
#endif
          t->size++;
#ifdef JSMN_PARENT_LINKS
          token->parent = parser->toksuper;
#endif
        }
        token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
        token->start = parser->pos;
        parser->toksuper = parser->toknext - 1;
        break;
      case '}':
      case ']':
        if (tokens == NULL)
        {
          break;
        }
        type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
        if (parser->toknext < 1)
        {
          return JSMN_ERROR_INVAL;
        }
        token = &tokens[parser->toknext - 1];
        for (;;)
        {
          if (token->start != -1 && token->end == -1)
          {
            if (token->type != type)
            {
              return JSMN_ERROR_INVAL;
            }
            token->end = parser->pos + 1;
            parser->toksuper = token->parent;
            break;
          }
          if (token->parent == -1)
          {
            if (token->type != type || parser->toksuper == -1)
            {
              return JSMN_ERROR_INVAL;
            }
            break;
          }
          token = &tokens[token->parent];
        }
#else
        for (i = parser->toknext - 1; i >= 0; i--)
        {
          token = &tokens[i];
          if (token->start != -1 && token->end == -1)
          {
            if (token->type != type)
            {
              return JSMN_ERROR_INVAL;
            }
            parser->toksuper = -1;
            token->end = parser->pos + 1;
            break;
          }
        }
        /* Error if unmatched closing bracket */
        if (i == -1)
        {
          return JSMN_ERROR_INVAL;
        }
        for (; i >= 0; i--)
        {
          token = &tokens[i];
          if (token->start != -1 && token->end == -1)
          {
            parser->toksuper = i;
            break;
          }
        }
#endif
        break;
      case '\"':
        r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
        if (r < 0)
        {
          return r;
        }
        count++;
        if (parser->toksuper != -1 && tokens != NULL)
        {
          tokens[parser->toksuper].size++;
        }
        break;
      case '\t':
      case '\r':
      case '\n':
      case ' ':
        break;
      case ':':
        parser->toksuper = parser->toknext - 1;
        break;
      case ',':
        if (tokens != NULL && parser->toksuper != -1 &&
            tokens[parser->toksuper].type != JSMN_ARRAY &&
            tokens[parser->toksuper].type != JSMN_OBJECT)
        {
#ifdef JSMN_PARENT_LINKS
          parser->toksuper = tokens[parser->toksuper].parent;
#else
          for (i = parser->toknext - 1; i >= 0; i--)
          {
            if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT)
            {
              if (tokens[i].start != -1 && tokens[i].end == -1)
              {
                parser->toksuper = i;
                break;
              }
            }
          }
#endif
        }
        break;
#ifdef JSMN_STRICT
      /* In strict mode primitives are: numbers and booleans */
      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case 't':
      case 'f':
      case 'n':
        /* And they must not be keys of the object */
        if (tokens != NULL && parser->toksuper != -1)
        {
          const jsmntok_t *t = &tokens[parser->toksuper];
          if (t->type == JSMN_OBJECT ||
              (t->type == JSMN_STRING && t->size != 0))
          {
            return JSMN_ERROR_INVAL;
          }
        }
#else
      /* In non-strict mode every unquoted value is a primitive */
      default:
#endif
        r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
        if (r < 0)
        {
          return r;
        }
        count++;
        if (parser->toksuper != -1 && tokens != NULL)
        {
          tokens[parser->toksuper].size++;
        }
        break;

#ifdef JSMN_STRICT
      /* Unexpected char in strict mode */
      default:
        return JSMN_ERROR_INVAL;
#endif
      }
    }

    if (tokens != NULL)
    {
      for (i = parser->toknext - 1; i >= 0; i--)
      {
        /* Unmatched opened object or array */
        if (tokens[i].start != -1 && tokens[i].end == -1)
        {
          return JSMN_ERROR_PART;
        }
      }
    }

    return count;
  }

  /**
   * Creates a new parser based over a given buffer with an array of tokens
   * available.
   */
  JSMN_API void jsmn_init(jsmn_parser *parser)
  {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
  }

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

/* Added in by JBlanked on 2024-10-16 for use in Flipper Zero SDK*/

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <furi.h>

// Helper function to compare JSON keys
int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0)
  {
    return 0;
  }
  return -1;
}

// return the value of the key in the JSON data
// works for the first level of the JSON data
char *get_json_value(char *key, char *json_data, uint32_t max_tokens)
{
  // Parse the JSON feed
  if (json_data != NULL)
  {
    jsmn_parser parser;
    jsmn_init(&parser);

    // Allocate tokens array on the heap
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * max_tokens);
    if (tokens == NULL)
    {
      FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
      return NULL;
    }

    int ret = jsmn_parse(&parser, json_data, strlen(json_data), tokens, max_tokens);
    if (ret < 0)
    {
      // Handle parsing errors
      FURI_LOG_E("JSMM.H", "Failed to parse JSON: %d", ret);
      free(tokens);
      return NULL;
    }

    // Ensure that the root element is an object
    if (ret < 1 || tokens[0].type != JSMN_OBJECT)
    {
      FURI_LOG_E("JSMM.H", "Root element is not an object.");
      free(tokens);
      return NULL;
    }

    // Loop through the tokens to find the key
    for (int i = 1; i < ret; i++)
    {
      if (jsoneq(json_data, &tokens[i], key) == 0)
      {
        // We found the key. Now, return the associated value.
        int length = tokens[i + 1].end - tokens[i + 1].start;
        char *value = malloc(length + 1);
        if (value == NULL)
        {
          FURI_LOG_E("JSMM.H", "Failed to allocate memory for value.");
          free(tokens);
          return NULL;
        }
        strncpy(value, json_data + tokens[i + 1].start, length);
        value[length] = '\0'; // Null-terminate the string

        free(tokens); // Free the token array
        return value; // Return the extracted value
      }
    }

    // Free the token array if key was not found
    free(tokens);
  }
  else
  {
    FURI_LOG_E("JSMM.H", "JSON data is NULL");
  }
  FURI_LOG_E("JSMM.H", "Failed to find the key in the JSON.");
  return NULL; // Return NULL if something goes wrong
}