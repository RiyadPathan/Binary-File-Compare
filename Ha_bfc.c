
#define UNIX
#define OLD_STYLE

#ifdef OLD_STYLE
#define _(X) ()
#else
#define _(X) X
#endif

#ifdef MSC
#include <io.h>
#endif
#ifdef UNIX
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define BUF_SIZE              1024
#define DEFAULT_BUF_SIZE       128
#define DEFAULT_TEXT_BUF_SIZE  512
#define TRUE     1
#define FALSE    0

#define MIN(x,y) ((x) < (y) ? (x) : (y))

typedef unsigned int word;
typedef unsigned long int longword;
typedef int bool;

word n_match_char;
word buffer_size;



typedef struct file_buf_T
   { FILE  *file;
     char  *file_name;
     char  *info; /* == buffer + pos */
     longword file_pos;
     longword file_len;
     word  len;
     char  buffer[2*BUF_SIZE];
     word  pos;
     word  filled;
   } file_buf_t;

bool only_text = FALSE,
     ignore_space = FALSE,
     ignore_case = FALSE;


/* prototypes: */

int main _((int argc, char *argv[]));
bool open_file _((file_buf_t *file_buf, char *filename));
void advance _((file_buf_t *file_buf, word steps));
void echo_16_bytes _((file_buf_t *file_buffer, word len));
void echo_bytes _((file_buf_t *file_buffer, word len));
int seek_seq _((file_buf_t *fb1, file_buf_t *fb2, word *p1, word *p2));
int seek_eq _((file_buf_t *fb1, file_buf_t *fb2, word *p1, word *p2));

#ifndef MSC
longword filelength _((FILE *f));
#endif

word buffer_size,
     min_match,
     max_match;


#ifndef OLD_STYLE
main(int argc, char *argv[])
#else
main(argc, argv) int argc; char *argv[];
#endif
{ file_buf_t fb1, fb2;
  bool no_diff = TRUE;
  bool show_all = FALSE;

  { char *file1 = NULL,
         *file2 = NULL;
    word i;
    bool error = FALSE;

    n_match_char = 1;
    buffer_size = DEFAULT_BUF_SIZE;
    min_match = 1;
    max_match = BUF_SIZE;

    for (i = 1; i < argc && !error ; i++)
        { int h;
          if   (argv[i][0] == '-')
               if   (toupper(argv[i][1]) == 'A' && argv[i][2] == '\0')
                    show_all = TRUE;
               else if (toupper(argv[i][1]) == 'B')
                    if   (!sscanf(argv[i]+2, "%d", &h) || h <= 0)
                         { printf("bfc: -Bnn option needs positive integer ");
                           printf(     "value.\n\n");
                           error = TRUE;
                         }
                    else if (h > BUF_SIZE)
                         { printf("bfc: buffer size larger than %d ",
                                  BUF_SIZE);
                           printf(     "not allowed; %d is used instead.\n",
                                  BUF_SIZE);
                           buffer_size = BUF_SIZE;
                         }
                    else
                         buffer_size = h;
               else if (toupper(argv[i][1]) == 'M')
                    if   (!sscanf(argv[i]+2, "%d", &h) || h <= 0)
                         { printf("bfc: -Mnn option needs positive integer ");
                           printf(     "value.\n\n");
                           error = TRUE;
                         }
                    else
                         max_match = h;
               else if (toupper(argv[i][1]) == 'S' && argv[i][2] == '\0')
                       ignore_space = TRUE;
               else if (toupper(argv[i][1]) == 'C' && argv[i][2] == '\0')
                       ignore_case = TRUE;
               else if (toupper(argv[i][1]) == 'T' && argv[i][2] == '\0')
                       { only_text = TRUE;
                         buffer_size = DEFAULT_TEXT_BUF_SIZE;
                       }
               else if (sscanf(argv[i]+1, "%d", &h))
                    if   (h <= 0)
                         { printf("bfc: -nn option needs positive integer ");
                           printf(      "value.\n\n");
                           error = TRUE;
                         }
                    else if (h >= BUF_SIZE - 1)
                         { printf("bfc: minimal required bytes for resync ");
                           printf(     "should be smaler than %d.\n",
                                  BUF_SIZE);
                           printf("     %d is used instead.\n",
                                  BUF_SIZE - 1);
                           min_match = BUF_SIZE - 1;
                         }
                    else
                         min_match = h;
               else
                    { printf("bfc: unknown option: %s\n\n", argv[i]);
                      error = TRUE;
                    }
          else if (file1 == NULL)
               file1 = argv[i];
          else if (file2 == NULL)
               file2 = argv[i];
          else
               { printf("bfc: too many arguments\n\n");
                 error = TRUE;
               }
        }
    if (!error && file2 == NULL)
       { printf("bfc: too few arguments.\n\n");
         error = TRUE;
       }
    if (error)
       { printf("Usages: bfc [-nn] [-Mnn] [-Bnn] [-A] [-T] file1 file2\n");
         if (argc < 2)
            printf("   -nn  : minimal number bytes (<%d) required for resync.",
                   BUF_SIZE);
            printf(         " (Default: 1)\n");
            printf("   -Mnn : number bytes sufficient for resync.\n");
            printf("   -Bnn : buffer size (<=%d) to be used. (Default: %d)\n",
                   BUF_SIZE, DEFAULT_BUF_SIZE);
            printf("   -S   : ignore differences in spaces\n");
            printf("   -C   : case insensitive\n");
            printf("   -A   : shows also equal parts\n");
            printf("   -T   : show only text representation (includes -B%d)\n",
                   DEFAULT_TEXT_BUF_SIZE);
         return 2;
       }

    if (min_match > buffer_size)
       { buffer_size = min_match + min_match / 2;
         if (buffer_size > BUF_SIZE)
            buffer_size = BUF_SIZE;
         printf("bfc: buffer size set to %d, because minimal number of ",
                buffer_size);
         printf(     "bytes required\n");
         printf("     for resync was set to %d\n", min_match);
       }
    if (max_match < min_match)
       { max_match = min_match;
         printf("bfc: number bytes sufficient for resync set to %d, because ",
                max_match);
         printf(     "minimal number\n");
         printf("     of bytes required for resync was set to %d.\n",
                min_match);
       }

    if (!open_file(&fb1, file1))
       { printf("bfc: cannot open %s - No such file\n", file1);
         return 2;
       }

    if (!open_file(&fb2, file2))
       { printf("bfc: cannot open %s - No such file\n", file2);
         return 2;
       }
  }

  if (min_match > buffer_size)
     { buffer_size = min_match + min_match / 2;
       if (buffer_size > BUF_SIZE)
          buffer_size = BUF_SIZE;
       printf("bfc: buffer size set to %d, because minimal number of ",
              buffer_size);
       printf(     "bytes required\n");
       printf("     for resync was set to %d\n", min_match);
     }
  if (max_match < min_match)
     { max_match = min_match;
       printf("bfc: number bytes sufficient for resync set to %d, because ",
              max_match);
       printf(     "minimal number\n");
       printf("     of bytes required for resync was set to %d.\n",
              min_match);
     }


  while (fb1.len > 0 && fb2.len > 0)
    { word len1, len2;
      if   (seek_eq(&fb1, &fb2, &len1, &len2))
           { if   (show_all)
                  echo_bytes(&fb1, len1);
             else
                  advance(&fb1, len1);
             advance(&fb2, len2);
           }
      else
           { word next_pos1, next_pos2;
             no_diff = FALSE;
             if   (seek_seq(&fb1, &fb2, &next_pos1, &next_pos2) >= min_match)
                  { if (next_pos1 > 0 || !only_text)
                       { printf(only_text ? "[* %s *]" : "***** %s\n",
                         fb1.file_name);
                         echo_bytes(&fb1, next_pos1);
                       }
                    if (next_pos2 > 0 || !only_text)
                       { printf(only_text ? "[* %s *]" : "***** %s\n",
                         fb2.file_name);
                         echo_bytes(&fb2, next_pos2);
                       }
                    if   (show_all)
                         printf(only_text ? "[* %s + %s *]" : "***** %s + %s\n",
                                fb1.file_name, fb2.file_name);
                    else
                         printf(  only_text
                                ? (show_all ? "[**]" : "[**]\n")
                                : "*****\n\n");
                  }
             else
                  { printf(  only_text
                           ? "[* Resinc failed. *]"
                           : "Resinc failed. Files are too different.\n");
                    printf(only_text ? "[* %s *]" : "***** %s\n", fb1.file_name);
                    echo_bytes(&fb1, MIN(buffer_size, fb1.len));
                    printf(only_text ? "[* %s *]" : "***** %s\n", fb2.file_name);
                    echo_bytes(&fb2, MIN(buffer_size, fb2.len));
                    printf(  only_text
                           ? (show_all ? "[**]" : "[**]\n")
                           : "*****\n\n");
                    return 1;
                  }
           }
    }

  if (fb1.len > 0 || fb2.len > 0)
     { printf(only_text ? "[* %s *]" : "***** %s\n", fb1.file_name);
       while (fb1.len > 0)
           { int len16 = MIN(fb1.len, 16);
             echo_16_bytes(&fb1, (word)len16);
             advance(&fb1, (word)len16);
           }
       printf(only_text ? "[* %s *]" : "***** %s\n", fb2.file_name);
       while (fb2.len > 0)
           { int len16 = MIN(fb2.len, 16);
             echo_16_bytes(&fb2, (word)len16);
             advance(&fb2, (word)len16);
           }
       printf(  only_text
              ? (show_all ? "[**]" : "[**]\n")
              : "*****\n\n");
     }
  if (no_diff)
     printf("bfc: no differences encountered\n");

  return no_diff ? 0 : 1;
}

#ifndef OLD_STYLE
int seek_seq(file_buf_t *fb1, file_buf_t *fb2, word *p1, word *p2)
#else
int seek_seq(fb1, fb2, p1, p2) file_buf_t *fb1, *fb2; word *p1, *p2;
#endif
{ word l1 = MIN(fb1->len, BUF_SIZE),
       l2 = MIN(fb2->len, BUF_SIZE),
       depth, i1, i2,
       max_len,
       max_depth;
  
  max_len = 0;
  max_depth = buffer_size * 2 - min_match;

  for (depth = 1;
          depth < max_depth
       && max_len < depth && max_len < max_match;
       depth++)
     for (i1 = 0, i2 = depth; i1 <= depth; i1++ , i2--)
         if (   i1 < l1 && i2 < l2
             && (   fb1->info[i1] == fb2->info[i2]
                 || (   ignore_case
                     && toupper(fb1->info[i1]) == toupper(fb2->info[i2]))))
            { int j1 = i1+1, j2 = i2+1, len = 1;
              while (   j1 < l1 && j2 < l2
                     && len < max_match
                     && len > 0
                     && (   fb1->info[j1] == fb2->info[j2]
                         || (   ignore_case
                             &&    toupper(fb1->info[j1])
                                == toupper(fb2->info[j2]))))
                    { j1++;
                      j2++;
                      len++;
                    }
              if (len > max_len && len >= min_match)
              { max_len = len;
                *p1 = i1;
                *p2 = i2;
              }
            }
  return max_len;
}

#ifndef OLD_STYLE
int seek_eq(file_buf_t *fb1, file_buf_t *fb2, word *p1, word *p2)
#else
int seek_eq(fb1, fb2, p1, p2) file_buf_t *fb1, *fb2; word *p1, *p2;
#endif
{ word l1 = MIN(fb1->len, 16),
       l2 = MIN(fb2->len, 16),
       i1, i2;
  i1 = 0;
  i2 = 0;
  while (i1 < l1 && i2 < l2)
     { if   (  ignore_case
             ? toupper(fb1->info[i1]) == toupper(fb2->info[i2])
             : fb1->info[i1] == fb2->info[i2]
            )
            { i1++;
              i2++;
            }
       else if (   ignore_space
                && (isspace(fb1->info[i1]) || isspace(fb2->info[i2])))
            { if (isspace(fb1->info[i1]))
                   i1++;
              if (isspace(fb2->info[i2]))
                   i2++;
            }
       else
            break;
     }

  *p1 = i1;
  *p2 = i2;

  return i1 > 0 || i2 > 0;
}

#ifndef OLD_STYLE
bool open_file(file_buf_t *file_buf, char *filename)
#else
bool open_file(file_buf, filename) file_buf_t *file_buf; char *filename;
#endif
{ file_buf->file = fopen(filename, "rb");
  if (file_buf->file == NULL)
     return FALSE;

  file_buf->file_name = (char *)malloc(strlen(filename) + 1);
  strcpy(file_buf->file_name, filename);
  file_buf->filled = read(fileno(file_buf->file), file_buf->buffer, 2*BUF_SIZE);
  file_buf->pos = 0;

  file_buf->info = file_buf->buffer + file_buf->pos;
  file_buf->file_pos = 0L;
  file_buf->file_len = filelength(file_buf->file);
  file_buf->len = file_buf->filled;

  return TRUE;
}


#ifndef OLD_STYLE
void advance(file_buf_t *file_buffer, word steps)
#else
void advance(file_buffer, steps) file_buf_t *file_buffer; word steps;
#endif
{ word i;
  for (i = 0; i < steps && file_buffer->len > 0; i++)
  { file_buffer->file_pos++;
    file_buffer->pos++;
    if (file_buffer->pos > BUF_SIZE)
       { memcpy(file_buffer->buffer, file_buffer->buffer + BUF_SIZE, BUF_SIZE);
         file_buffer->pos -= BUF_SIZE;
         file_buffer->filled -= BUF_SIZE;
         if (file_buffer->filled == BUF_SIZE)
            file_buffer->filled += read(fileno(file_buffer->file),
                                        file_buffer->buffer + BUF_SIZE,
                                        BUF_SIZE);

       }
    file_buffer->info = file_buffer->buffer + file_buffer->pos;
    file_buffer->len  = file_buffer->filled - file_buffer->pos;
  }
}

#ifndef OLD_STYLE
void echo_16_bytes(file_buf_t *file_buffer, word len)
#else
void echo_16_bytes(file_buffer,len) file_buf_t *file_buffer; word len;
#endif
{ if (only_text)
       { int i;
         for (i = 0; i < len; i++)
             { char c = file_buffer->info[i];
               if   (c == '\n')
                    printf("\n");
               else if (c == '\015')
                    ;
               else if (c < ' ' && c >= 0)
                    printf("\\%03o", c);
               else
                    printf("%c", c);
             }
       }
  else
       { int i;
         printf("%05lX  ", file_buffer->file_pos);
         for (i = 0; i < 16; i++)
             { if (i > 0 && i % 4 == 0)
                    printf("³ ");
               if (i < len)
                    { int w = file_buffer->info[i];
                      if (w < 0)
                         w += 256;
                      printf("%02X ", w);
                    }
               else
                    printf("   ");
             }
         printf("  ");
         for (i = 0; i < len; i++)
             { char c = file_buffer->info[i];
               if (c < ' ' && c >= 0)
                    printf(".", c);
               else
                    printf("%c", c);
             }
         printf("\n");
       }
}

#ifndef OLD_STYLE
void echo_bytes(file_buf_t *file_buffer, word len)
#else
void echo_bytes(file_buffer, len) file_buf_t *file_buffer; word len;
#endif
{ while(len > 0)
    { int len16 = MIN(len, 16);
      echo_16_bytes(file_buffer, (word)len16);
      advance(file_buffer, (word)len16);
      len -= len16;
    }
}

#ifndef MSCC
#ifndef OLD_STYLE
longword filelength(FILE *f)
#else
longword filelength(f) FILE *f;
#endif
{   int fh = fileno(f);
    longword cur_pos,
             result;

    cur_pos = lseek(fh, 0L, SEEK_CUR);
    result = lseek(fh, 0L, SEEK_END);
    lseek(fh, (long int)cur_pos, SEEK_SET);

    return result;
}
#endif
