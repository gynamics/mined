/**
 * mined.c - mini editor
 *
 * A minimal interactive ASCII text editor.
 * CopyRevolted by gynamics
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define LBSZ 4096
struct winsize wsz; // window size
int fd = -1;        // fd and mmap of edited file
int cl = 1;         // current line
off_t foff = 0;     // file offset
char fbu[LBSZ];     // file buffer
char *hist[256];    // history ring
int hi = 0;         // history increment register
int hp = 0;         // history pointer
char buf[LBSZ + 1]; // line buffer
int vpts[LBSZ + 1]; // visual points
int vi = 0;         // visual increment register
int bi = 0;         // buffer increment register
int bp = 0;         // buffer top
char cb[LBSZ];      // clipboard buffer
int cbp = 0;        // clipboard top

int goto_line(int n) {
  int fln = 1;
  int loff = 0;
  int fo = lseek(fd, 0, SEEK_SET);
  if (n == 1)
    return 1;
  for (;;) {
    int len = read(fd, fbu, LBSZ);
    char *p;
    for (p = fbu, ++loff; p < fbu + len; ++p, ++loff) {
      if (*p == '\n') {
        fo += loff;
        loff = 0;
        if (++fln == n) {
          lseek(fd, fo, SEEK_SET);
          return fln;
        }
      }
    }
    if (len < LBSZ) { // EOF
      lseek(fd, fo, SEEK_SET);
      return fln;
    }
  }
}

int goto_char(int n) {
  int loff = 0;
  int fo = foff;
  if (n == 0)
    return 0;
  for (;;) {
    int len = read(fd, fbu, LBSZ);
    char *p;
    for (p = fbu, ++loff; p < fbu + len; ++p, ++loff) {
      if (loff == n) {
        fo += loff;
        lseek(fd, fo, SEEK_SET);
        return loff;
      } else if (*p == '\n') {
        if (loff > 0)
          fo += (n % loff + loff) % loff;
        lseek(fd, fo, SEEK_SET);
        return fo - foff;
      }
    }
    if (len < LBSZ) // EOF
      return loff;
  }
}

int ilg(int x) { return x >= 10 ? 1 + ilg(x / 10) : 1; }

void print_lines(int lb, int le) {
  if (le < 0) {
    int m = goto_line(lb);
    lb = (lb > 0) ? lb : m;
    le = lb;
  } else { // ugly, but I have no idea about how to determine W
    le = goto_line(le);
    int m = goto_line(lb);
    lb = (lb > 0) ? lb : m;
  }
  int w = ilg(le);
  printf("\e[7m%*d:\e[0m", w, lb);
  off_t off = lseek(fd, 0, SEEK_CUR);
  for (;;) {
    int len = read(fd, fbu, LBSZ);
    char *p;
    for (p = fbu; p < fbu + len; ++p, ++off) {
      printf((off != foff) ? "%c"
                           : (*p != '\n' ? "\e[7m%c\e[0m" : "\e[7m %c\e[0m"),
             *p);
      if (*p == '\n') {
        if (lb++ == le)
          return;
        if (off == foff)
          printf("\e[7m \e[0m");
        printf("\e[7m%*d:\e[0m", w, lb);
      }
    }
    if (len < LBSZ) { // EOF
      printf("\e[7mEOF\e[0m\n");
      return;
    }
  }
}

void foreward_shift(off_t fb, int d) {
  off_t fe = lseek(fd, 0, SEEK_END);
  if (fb < fe) {
    int sz;
    do {
      sz = (fe - fb > LBSZ) ? LBSZ : (fe - fb);
      fe -= sz;
      lseek(fd, fe, SEEK_SET);
      read(fd, fbu, sz);
      lseek(fd, fe + d, SEEK_SET);
      write(fd, fbu, sz);
    } while (sz == LBSZ);
  }
}

void backward_shift(off_t fb, int be) {
  off_t fe = lseek(fd, 0, SEEK_END);
  if (be < fe) {
    for (;; fb += LBSZ, be += LBSZ) {
      lseek(fd, be, SEEK_SET);
      int len = read(fd, fbu, LBSZ);
      lseek(fd, fb, SEEK_SET);
      write(fd, fbu, len);
      if (len < LBSZ) // EOF
        break;
    }
  }
  ftruncate(fd, fe - (be - fb));
}

int e_off = -1;
int e_len = 0;
int commit_edited_line(char *s) {
  int len = strlen(s);
  int d = len - e_len;
  if (d > 0) {
    foreward_shift(e_off, d);
  } else if (d < 0) {
    backward_shift(e_off + len, e_off + e_len);
  }
  lseek(fd, e_off, SEEK_SET);
  write(fd, s, len);
  return 0;
}

static inline char *_next_arg(char *arg) {
  while (*arg != '\0' && strchr(" \t\v\n", *arg))
    ++arg;
  return arg;
}
#define NA(x, args) x = _next_arg(args)

#define NUM(n, ln)                                                             \
  NA(n, args);                                                                 \
  ln = strtol(n, &args, 0);                                                    \
  if (!ln && n == args) {                                                      \
    if (*n == '$')                                                             \
      args++, ln = 0;                                                          \
    else                                                                       \
      ln = -1;                                                                 \
  }

#define NUM1(ln)                                                               \
  char *n;                                                                     \
  int ln = -1;                                                                 \
  NUM(n, ln)                                                                   \
  if (ln < 0)                                                                  \
    return -1;

#define NUM2(lb, sep, le)                                                      \
  char *n;                                                                     \
  int lb = -1, le = -1;                                                        \
  NUM(n, lb)                                                                   \
  if (lb < 0)                                                                  \
    return -1;                                                                 \
  NA(args, args);                                                              \
  if (*args == sep) {                                                          \
    args++;                                                                    \
    NUM(n, le)                                                                 \
  }

#define ENSURE_FD                                                              \
  if (fd < 0) {                                                                \
    printf("NO FILE OPENED\n.");                                               \
    return -1;                                                                 \
  }

#define QUERY(question, action)                                                \
  printf question;                                                             \
  fflush(stdout);                                                              \
  if (getchar() == 'y') {                                                      \
    printf("\n");                                                              \
    action;                                                                    \
  } else {                                                                     \
    printf("\nABORT\n");                                                       \
    return -1;                                                                 \
  }

int mined_c() {
  if (fd > 0) {
    close(fd);
    fd = -1;
    return 0;
  } else
    return -1;
}

int mined_o(char *args) {
  NA(char *n, args);
  mined_c(); // reset fd status
  if (access(n, F_OK)) {
    QUERY(("%s NOT EXIST, CREATE?(y/n)", n),
          fd = open(n, O_CREAT | O_RDWR, 0666))
  } else
    fd = open(n, O_RDWR);
  if (fd >= 0) {
    printf("FD = %d\n", fd);
    return 0;
  } else
    return fd;
}

void anl(int d) {
  int cnt = 0;
  lseek(fd, 0, SEEK_END);
  while (cnt < d) {
    char *p;
    for (p = fbu; p < fbu + LBSZ && cnt < d; p++) {
      cnt++;
      *p = '\n';
    }
    write(fd, fbu, p - fbu);
  }
  lseek(fd, 0, SEEK_END);
}

int mined_d(char *args) {
  ENSURE_FD;
  NUM2(lb, ',', le);
  off_t fb, be;
  goto_line(lb);
  fb = lseek(fd, 0, SEEK_CUR);
  if (le == 0 || (!lb && le < 0)) {
    // special case: deleting the last line
    be = lseek(fd, 0, SEEK_END);
    --fb;
  } else {
    goto_line((le > 0) ? 1 + le : 1 + lb);
    be = lseek(fd, 0, SEEK_CUR);
  }
  if (fb <= be) {
    if (fb < be)
      backward_shift(fb, be);
    print_lines(lb, lb + 1);
    return 0;
  } else {
    printf("INVALID RANGE: %lu,%lu\n", fb, be);
    return -1;
  }
}

int mined_x(char *args) {
  ENSURE_FD;
  NUM1(nc);
  backward_shift(foff, foff + nc);
  print_lines(cl, cl);
  return 0;
}

int mined_i(char *args) {
  ENSURE_FD;
  int len = strlen(args);
  lseek(fd, foff, SEEK_SET);
  foreward_shift(foff, 1 + len);
  lseek(fd, foff, SEEK_SET);
  args[len] = '\n';
  write(fd, args, 1 + len);
  foff += (1 + len);
  print_lines(cl, 1 + cl);
  ++cl;
  return 0;
}

int mined_a(char *args) {
  ENSURE_FD;
  int len = strlen(args);
  lseek(fd, foff, SEEK_SET);
  foreward_shift(foff, len);
  lseek(fd, foff, SEEK_SET);
  write(fd, args, len);
  foff += len;
  print_lines(cl, cl);
  return 0;
}

int mined_p(char *args) {
  ENSURE_FD;
  NUM2(lb, ',', le);
  print_lines(lb, le);
  return 0;
}

int mined_g(char *args) {
  ENSURE_FD;
  NUM2(rn, ':', cn);
  int m = goto_line(rn);
  if (m < rn) {
    QUERY(("LINE %d NOT EXIST, CREATE IT?(y/n)", rn), anl(rn - m))
  }
  cl = m;
  foff = lseek(fd, 0, SEEK_CUR); // save file offset
  foff += goto_char(cn);
  printf("GOTO %d:%d, FILE OFFSET=%ld\n", rn, cn, foff);
  print_lines(rn, rn);
  return 0;
}

void move_and_print(int from, int len) {
  int wcl = wsz.ws_col;
  for (int j = from; j < from + len; ++j) {
    if (buf[j] == '\t') {
      vpts[j + 1] = 8 * (1 + vpts[j] / 8);
      if (vpts[j + 1] > wcl)
        vpts[j + 1] = wcl;
      for (int i = vpts[j]; i < vpts[j + 1]; i++)
        printf(" ");
    } else if (vpts[j] < wcl) {
      vpts[j + 1] = vpts[j] + 1;
      printf("%c", buf[j]);
    } else {
      vpts[j + 1] = vpts[j];
    }
  }
}

static inline void set_cursor(int n) {
  bi = n;
  printf("\e[%dG\e[K", vpts[bi]);
}

void load_line(char *s, int len) {
  memcpy(buf, s, len);
  vpts[0] = 1;
  set_cursor(0);
  move_and_print(bi, len);
  bi = bp = len;
}

int e_mode = 0;
int mined_e(char *args) {
  ENSURE_FD;
  NUM2(rn, ':', cn);
  int m = goto_line(rn);
  if (m < rn) { // insert newlines until LN
    QUERY(("LINE %d NOT EXIST, CREATE IT?(y/n)", rn), anl(rn - m))
  }
  goto_char(cn);
  e_mode = 1;
  e_off = lseek(fd, 0, SEEK_CUR);
  if (m < rn)
    e_len = 0;
  else {
    int len = read(fd, fbu, LBSZ);
    char *p;
    for (p = fbu; p < fbu + len && *p != '\n'; p++)
      ;
    if (p == fbu + LBSZ) {
      printf("LINE TOO LONG, ABORT.\n");
      return 0;
    }
    e_len = p - fbu;
  }
  load_line(fbu, e_len);
  fflush(stdout);
  return 0;
}

int (*mined_cmds[256])(char *) = {
    ['o'] = mined_o, ['g'] = mined_g, ['i'] = mined_i, ['d'] = mined_d,
    ['p'] = mined_p, ['e'] = mined_e, ['a'] = mined_a, ['x'] = mined_x,
};

int mined_eval(char *args) {
#ifdef DEBUG
  printf("%s\n", buf);
  for (int j = 0; buf[j] != '\0'; ++j)
    printf("%02x ", buf[j]);
  printf("\n");
#endif
  char *n;
  NA(n, args);
  int (*f)(char *) = mined_cmds[*(unsigned char *)n];
  if (f)
    return f(n + 1);
  else
    return -1;
}

static inline void move_left(int n) {
  bi = (bi > n) ? bi - n : 0;
  printf("\e[%dG", vpts[bi]);
}

static inline void move_right(int n) {
  bi = (bi + n < bp) ? bi + n : bp;
  printf("\e[%dG", vpts[bi]);
}

static inline void update_tail() {
  move_and_print(bi, bp - bi);
  printf("\e[K\e[%dG", vpts[bi]);
}

void insert_char(int c) {
  if ((c >= 0x20 || c == '\t')) {
    if (bi < bp)
      for (int j = bp; j > bi; --j)
        buf[j] = buf[j - 1];
    ++bp;
    buf[bi] = c;
    move_and_print(bi, 1);
    ++bi;
    if (bi < bp)
      update_tail();
  }
}

void delete_foreward(int n) {
  if (bi + n < bp) {
    for (int j = bi + n; j < bp; ++j)
      buf[j - n] = buf[j];
    bp -= n;
    update_tail();
  } else if (bi < bp) {
    bp = bi;
    fputs("\e[K", stdout);
  }
}

void delete_backward(int n) {
  n = (bi >= n) ? n : bi;
  if (n > 0) {
    for (int j = bi; j < bp; ++j)
      buf[j - n] = buf[j];
    bp -= n;
    move_left(n);
    update_tail();
  }
}

void load_hist(int j) {
  char *l = hist[j];
  hi = j;
  int len = strlen(l);
  strcpy(buf, l);
  set_cursor(0);
  move_and_print(bi, len);
  bi = bp = len;
}

void last_hist() {
  int j = (hi + 255) % 256;
  if (hist[j])
    load_hist(j);
}

void next_hist() {
  int j = (hi + 1) % 256;
  if (hist[j])
    load_hist(j);
  else if (hist[hi]) { // empty record
    if (e_mode) {
      load_line(fbu, e_len);
    } else {
      hi = j;
      set_cursor(0);
      bp = 0;
    }
  }
}

void kill_ring_save() {
  cbp = bp - bi;
  memcpy(cb, buf + bi, cbp);
  fputs("\e[K", stdout);
  bp = bi;
}

void kill_ring_yank() {
  if (cbp > 0) {
    for (int j = bp - 1; j >= bi; --j)
      buf[j + cbp] = buf[j];
    memcpy(buf + bi, cb, cbp);
    move_and_print(bi, cbp);
    bi += cbp;
    bp += cbp;
    update_tail();
  }
}

// enable nonblock getc
static struct termios tccfg[2];
void enable_raw_mode() {
  tcgetattr(0, &tccfg[0]);
  tccfg[1] = tccfg[0];
  tccfg[1].c_lflag &= ~(ICANON | ECHO);
  tccfg[1].c_cc[VTIME] = 0;
  tccfg[1].c_cc[VMIN] = 1;
  tcsetattr(0, TCSANOW, &tccfg[1]);
}

void onexit() {
  mined_c();
  tcsetattr(0, TCSANOW, &tccfg[0]);
}

// no multibyte support yet
int mined_repl(char *prompt) {
  char c;
  int vp0 = 1 + strlen(prompt);
  for (;;) {
  read:
    ioctl(1, TIOCGWINSZ, &wsz);
    if (!e_mode) {
      printf("%s", prompt);
      fflush(stdout);
      vpts[0] = vp0;
      bi = 0;
      bp = 0;
    }
    while ((c = getchar())) {
      switch (c) {
      case 0x01: // SOH, C-a
        move_left(bi);
        break;
      case 0x02: // STX, C-b
        move_left(1);
        break;
      case 0x05: // ENQ, C-e
        move_right(bp - bi);
        break;
      case 0x06: // ACK, C-f
        move_right(1);
        break;
      case 0x07: // \a, C-g
        e_mode = 0;
        e_off = -1;
        e_len = 0;
        move_right(bp - bi);
        printf("\nABORT\n");
        goto read;
      case 0x0b: // \v, C-k
        kill_ring_save();
        break;
      case 0x0e: // SO, C-n
        next_hist();
        break;
      case 0x10: // DLE, C-p
        last_hist();
        break;
      case 0x19: // EM, C-y
        kill_ring_yank();
        break;
      case 0x1b: { // ESC
        char c1 = getchar();
        if (c1 == '[') {
          char c2 = getchar();
          switch (c2) {
          case 'A': // UP
            last_hist();
            break;
          case 'B': // DOWN
            next_hist();
            break;
          case 'C': // RIGHT
            move_right(1);
            break;
          case 'D': // LEFT
            move_left(1);
            break;
          case '3':
            if (getchar() == '~') // DEL
              delete_foreward(1);
            break;
          default:
            break;
          }
        }
      } break;
      case 0x08: // \b
      case 0x7f: // backspace
        delete_backward(1);
        break;
      case 0x0a: // \n
        putchar('\n');
        buf[bp] = '\0';
        goto eval;
      default: // just echo if it is safe
        insert_char(c);
        break;
      }
      fflush(stdout); // flush output immdiately
    }
  eval:
    if (e_mode) {
      commit_edited_line(buf);
      e_mode = 0;
      e_off = -1;
      e_len = 0;
    } else {
      if (hist[hp])
        free(hist[hp]);
      hist[hp] = malloc(1 + bp);
      strcpy(hist[hp], buf);
      hi = hp = (hp + 1) % 256;
      int ret = mined_eval(buf);
      if (ret)
        printf("ERROR %d\n", ret);
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  enable_raw_mode();
  atexit(onexit);
  return mined_repl("M ");
}
