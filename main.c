#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include "spiffs.h"

static spiffs fs;
static uint8_t *flash;

#define LOG_PAGE_SIZE 256

static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];

static s32_t flash_read (u32_t addr, u32_t size, u8_t *dst) {
  memcpy (dst, flash + addr, size);
  return SPIFFS_OK;
}

static s32_t flash_write (u32_t addr, u32_t size, u8_t *src) {
  memcpy (flash + addr, src, size);
  return SPIFFS_OK;
}

static s32_t flash_erase (u32_t addr, u32_t size) {
  memset (flash + addr, 0xff, size);
  return SPIFFS_OK;
}


void die (const char *what)
{
  perror (what);
  exit (1);
}

int main (int argc, char *argv[])
{
  int opt;
  const char *fname = 0;
  bool create = false;
  enum { CMD_LIST, CMD_INTERACTIVE, CMD_SCRIPT } command;
  size_t sz = 0;
  while ((opt = getopt (argc, argv, "f:c:lir")) != -1)
  {
    switch (opt)
    {
      case 'f': fname = optarg; break;
      case 'c': create = true; sz = strtoul (optarg, 0, 0); break;
      case 'l': command = CMD_LIST; break;
      case 'i': command = CMD_INTERACTIVE; break;
      case 'r': command = CMD_SCRIPT; break;
      default: die ("unknown option");
    }
  }

  int fd = open (fname, (create ? (O_CREAT | O_TRUNC) : 0) | O_RDWR, 0660);
  if (fd == -1)
    die ("open");

  if (create)
  {
    if (lseek (fd, sz -1, SEEK_SET) == -1)
      die ("lseek");
    if (write (fd, "", 1) != 1)
      die ("write");
  }
  else if (!sz)
  {
    off_t offs = lseek (fd, 0, SEEK_END);
    if (offs == -1)
      die ("lseek");
    sz = offs;
  }

  if (sz & (LOG_PAGE_SIZE -1))
    die ("file size not multiple of page size");

  flash = mmap (0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (!flash)
      die ("mmap");

  if (create)
    memset (flash, 0xff, sz);

  spiffs_config cfg;
  cfg.phys_size = sz;
  cfg.phys_addr = 0;
  cfg.phys_erase_block = 0x1000;
  cfg.log_block_size = 0x1000;
  cfg.log_page_size = LOG_PAGE_SIZE;
  cfg.hal_read_f = flash_read;
  cfg.hal_write_f = flash_write;
  cfg.hal_erase_f = flash_erase;
  if (SPIFFS_mount (&fs, &cfg,
      spiffs_work_buf,
      spiffs_fds,
      sizeof(spiffs_fds),
      0, 0, 0) != 0)
    die ("spiffs_mount");

  // TODO: import/export support
  spiffs_file fh = SPIFFS_open (&fs, "foo/bar", SPIFFS_CREAT | SPIFFS_RDWR, 0);
  if (SPIFFS_write (&fs, fh, "hello, world\n", 12) < 0)
    die ("spiffs_write");
  SPIFFS_close (&fs, fh);

  if (command == CMD_LIST)
  {
    spiffs_DIR dir;
    if (!SPIFFS_opendir (&fs, "/", &dir))
      die ("spiffs_opendir");
    struct spiffs_dirent de;
    while (SPIFFS_readdir (&dir, &de))
    {
      static const char types[] = "?fdhs"; // file, dir, hardlink, softlink
      char name[sizeof(de.name)+1] = { 0 };
      memcpy (name, de.name, sizeof(de.name));
      printf("%c %6u %s\n", types[de.type], de.size, name);
    }
    SPIFFS_closedir (&dir);
  }

  munmap (flash, sz);
  return 0;
}
