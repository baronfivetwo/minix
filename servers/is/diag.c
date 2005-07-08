
#include <stdio.h>
#include <fcntl.h>

#include "is.h"
#include "../../kernel/const.h"
#include "../../kernel/type.h"

/*==========================================================================*
 *				log_message				    *
 *==========================================================================*/
PRIVATE void log_message(char *buf)
{
#if ENABLE_LOG
	message m;

	m.m_type = DIAGNOSTICS;
	m.DIAG_PRINT_BUF = buf;
	m.DIAG_BUF_COUNT = strlen(buf);

	_sendrec(LOG_PROC_NR, &m);
#endif

	return;
}


/*==========================================================================*
 *				do_new_kmess				    *
 *==========================================================================*/
PUBLIC int do_new_kmess(m)
message *m;					/* notification message */
{
/* Notification for a new kernel message. */
  struct kmessages kmess;		/* entire kmess structure */
  char print_buf[KMESS_BUF_SIZE];	/* copy new message here */
  static int prev_next = 0;
  int size, next;
  int bytes;
  int i, r;

  /* Try to get a fresh copy of the buffer with kernel messages. */
  if ((r=sys_getkmessages(&kmess)) != OK) {
  	report("IS","couldn't get copy of kmessages", r);
  	return;
  }

  /* Print only the new part. Determine how many new bytes there are with 
   * help of the current and previous 'next' index. Note that the kernel
   * buffer is circular. This works fine if less then KMESS_BUF_SIZE bytes
   * is new data; else we miss % KMESS_BUF_SIZE here.  
   * Check for size being positive, the buffer might as well be emptied!
   */
  if (kmess.km_size > 0) {
      bytes = ((kmess.km_next + KMESS_BUF_SIZE) - prev_next) % KMESS_BUF_SIZE;
      r=prev_next;				/* start at previous old */ 
      i=0;
      while (bytes > 0) {			
          print_buf[i] =  kmess.km_buf[(r%KMESS_BUF_SIZE)];
          diag_putc( kmess.km_buf[(r%KMESS_BUF_SIZE)] );
          bytes --;
          r ++;
          i ++;
      }
      /* Now terminate the new message and print it. */
      print_buf[i] = 0;
      printf(print_buf);

	/* Also send message to log device. */
	log_message(print_buf);
  }

  /* Almost done, store 'next' so that we can determine what part of the
   * kernel messages buffer to print next time a notification arrives.
   */
  prev_next = next;
  return EDONTREPLY;
}


/*===========================================================================*
 *				do_diagnostics				     *
 *===========================================================================*/
PUBLIC int do_diagnostics(message *m)
{
/* The IS server handles all diagnostic messages from servers and device 
 * drivers. It forwards the message to the TTY driver to display it to the
 * user. It also saves a copy in a local buffer so that messages can be 
 * reviewed at a later time.
 */
  int result;
  int proc_nr; 
  vir_bytes src;
  int count;
  char c;
  int i = 0;
  static char diagbuf[1024];

  /* Forward the message to the TTY driver. Inform the TTY driver about the
   * original sender, so that it knows where the buffer to be printed is.
   * The message type, DIAGNOSTICS, remains the same.
   */ 
  if ((proc_nr = m->DIAG_PROC_NR) == SELF)
      m->DIAG_PROC_NR = proc_nr = m->m_source;
  result = _sendrec(TTY, m);

  /* Now also make a copy for the private buffer at the IS server, so
   * that the messages can be reviewed at a later time.
   */
  src = (vir_bytes) m->DIAG_PRINT_BUF;
  count = m->DIAG_BUF_COUNT; 
  while (count > 0) {
      if (sys_datacopy(proc_nr, src, SELF, (vir_bytes) &c, 1) != OK) 
          break;		/* stop copying on error */
      diag_putc(c);		/* accumulate character */
      src ++;
      count --;
      diagbuf[i++] = c;
      if(i == sizeof(diagbuf) - 1) {
      	diagbuf[i] = '\0';
      	log_message(diagbuf);
      	i = 0;
      }
  }

  if(i > 0) {
  	/* This is safe; if i were too large,
  	 * this would have been done above.
  	 */
      	diagbuf[i] = '\0';
      	log_message(diagbuf);
  }

  return result;
}


/*===========================================================================*
 *				diag_putc				     *
 *===========================================================================*/
PUBLIC void diag_putc(c)
int c;					/* char to be added to diag buffer */
{
  diag_buf[diag_next] = c;
  diag_next = (diag_next + 1) % DIAG_BUF_SIZE;
  if (diag_size < DIAG_BUF_SIZE) 
      diag_size += 1;
}


/*===========================================================================*
 *				diagnostics_dmp				     *
 *===========================================================================*/
PUBLIC void diagnostics_dmp()
{
  char print_buf[DIAG_BUF_SIZE+1];	/* buffer used to print */
  int start;				/* calculate start of messages */
  int size, r;

  /* Reprint all diagnostic messages. First determine start and copy the
   * buffer into a print-buffer. This is done because the messages in the
   * copy may wrap (the buffer is circular).
   */
  start = ((diag_next + DIAG_BUF_SIZE) - diag_size) % DIAG_BUF_SIZE;
  r = 0;
  size = diag_size;
  while (size > 0) {
  	print_buf[r] = diag_buf[(start+r) % DIAG_BUF_SIZE];
  	r ++;
  	size --;
  }
  print_buf[r] = 0;		/* make sure it terminates */
  printf("Dump of diagnostics from device drivers and servers.\n\n"); 
  printf(print_buf);		/* print the messages */
}

