//////////////////////////////////////////////////////////////////////
//                             PMC-Sierra, Inc.
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Stephen Bates
//
//   Description:
//     Test function for the new preadv2 function and the priority
//     flag that goes with it.
//
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/time.h>

#include <linux/types.h>

#ifdef CONFIG_PREADV2

  /*
   * Note that since for now the SYS_preadv2 macro does not exist in
   * glibc we hard-code it to the value in
   * arch/x86/entry/syscalls/syscall_64.tbl. Livin' on the edge!!
   */
#define SYS_preadv2  326
#define SYS_pwritev2 327

#endif

  /*
   * A hack for now as I seem to have issues getting to the uapi
   * fs.h. Will fix this once I get this resolved.
   */
#define RWF_HIPRI  0x00000001

struct thread_info {
  pthread_t      thread_id;
  int            thread_num;

  unsigned long  fd;
  unsigned long  len;
  char           filename[256];
  struct iovec   *vec;
  unsigned long  flags;
  struct timeval starttime;
  struct timeval endtime;
};

static double timeval_to_secs(struct timeval *t)
{
    return  t->tv_sec + t->tv_usec / 1e6;
}

static void report(struct timeval starttime, struct timeval endtime,
		   size_t bytes)
{
  double elapsed_time = timeval_to_secs(&endtime) -
    timeval_to_secs(&starttime);

  fprintf(stdout, "report: exchanged %zd Bytes in %2.3f seconds.\n",
	  bytes, elapsed_time);
}

static void run(struct thread_info *tinfo)
{

  tinfo->fd = open(tinfo->filename, O_RDWR | O_TRUNC |
		   O_CREAT );
  if ( tinfo->fd == -1 ){
    fprintf(stderr,"error: open(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }
  gettimeofday(&tinfo->starttime, NULL);

  tinfo->vec = malloc(sizeof(struct iovec));
  if ( tinfo->vec == NULL ){
    fprintf(stderr,"error: malloc(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }
  tinfo->vec->iov_base = malloc(tinfo->len);
  tinfo->vec->iov_len  = tinfo->len;

#ifdef CONFIG_PREADV2

  syscall(SYS_pwritev2, tinfo->fd, tinfo->vec, 1, 0, 0,
	  tinfo->flags);

#else

  syscall(SYS_pwritev, tinfo->fd, tinfo->vec, 1, 0, 0);

#endif

  gettimeofday(&tinfo->endtime, NULL);
  free(tinfo->vec);
  close(tinfo->fd);
}

static void *thread_start(void *arg)
{
  struct thread_info *tinfo = arg;

  fprintf(stdout,"info: this is thread %d\n",
	  tinfo->thread_num);
  run(tinfo);

  return 0;

}

int main(int argc, char **argv)
{

  unsigned long fd, len = 8192, pos_l  = 0, pos_h = 0;
  struct iovec *vec;
  __u32 *buf, check;

  if ( argc !=2 ){
    fprintf(stderr,"error: %s takes one argument!\n",
	    argv[0]);
    exit(-1);
  }

  /*
   * Open the requested file in read-only mode. We will test the new
   * system call by reading from this file.
   */

  fd = open(argv[1], O_RDONLY);
  if ( fd == -1 ){
    fprintf(stderr,"error: open(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }

  /*
   * Create a simple iovec to act as a destination for the read. The
   * populate the members of that vector.
   */

  vec = malloc(sizeof(struct iovec));
  if ( vec == NULL ){
    fprintf(stderr,"error: malloc(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }
  vec->iov_base = malloc(len);
  vec->iov_len  = len;

#ifdef CONFIG_PREADV2

  syscall(SYS_preadv2, fd, vec, 1, pos_l, pos_h, RWF_HIPRI);

#else

  syscall(SYS_preadv, fd, vec, 1, pos_l, pos_h);

#endif

  if ( errno ){
    fprintf(stderr,"error: syscall(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }

  /*
   * Now do a check by re-opening the target file and doing a more
   * standard read and comparing the bits in the iovec buffer.
   */

  if ( lseek(fd, 0, SEEK_SET) == -1 ){
    fprintf(stderr,"error: lseek(): %s (%d)\n", strerror(errno),
	    errno);
    exit(errno);
  }

  buf = (__u32 *)vec->iov_base;
  for (unsigned i=0; i<len/4; i++){
    read(fd, &check, 4);
    if ( buf[i] != check ){
      fprintf(stderr,"error: mismatch: %d (0x%08x != 0x%08x)\n",
	      i, buf[i], check);
      exit(errno);
      }
  }
  fprintf(stdout,"check: all %ld entries match!\n", len/4);

  /*
   * Now that we have got this far we do a simple test that kicks off
   * two worker threads. One thread does low priority IO and the other
   * does high priority and we time each. Note that this is only going
   * to work on NVMe drives with DIRECT I/O for now.
   */
  struct thread_info tinfo[2];
  for ( unsigned i=0 ; i<2 ; i++){

    tinfo[i].thread_num = i;
    tinfo[i].len        = (1<<24);
    tinfo[i].flags = (i == 1) ? RWF_HIPRI : 0;
    sprintf(tinfo[i].filename, "test%d.dat", i);

    if ( pthread_create(&tinfo[i].thread_id, NULL, thread_start, &tinfo[i]) ){
      fprintf(stderr,"error: pthread_create(): %s (%d)\n", strerror(errno),
	      errno);
      exit(errno);
    }
  }

  for ( unsigned i = 0; i < 2; i++) {
    pthread_join(tinfo[i].thread_id, NULL);
  }

  report(tinfo[0].starttime, tinfo[0].endtime, 2*tinfo[0].len);
  report(tinfo[1].starttime, tinfo[1].endtime, 2*tinfo[1].len);

  free(vec->iov_base);
  free(vec);
  close(fd);

  pthread_exit(NULL);
  return 0;
}
