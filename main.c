/*
    Copyright (C) 2010  Daniel Richman

    Some code in the icon functions was based off XChat, which is 
      Copyright (C) 2006-2007 Peter Zelezny.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    For a full copy of the GNU General Public License, 
    see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include "irssi.h"
#include "irssi_alert.h"

GtkStatusIcon *status_icon;
GdkPixbuf *icon_idle, *icon_alert;

void *gtk_thread(void *data)
{
  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();
  exit(EXIT_FAILURE);
  return NULL;
}

void *reaper_thread(void *data)
{
  pid_t cpid, r;
  int status;

  cpid = *((pid_t *) data);

  for (;;)
  {
    r = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
    if (r == -1 || WIFEXITED(status) || WIFSIGNALED(status))
    {
      gdk_threads_enter();
      gtk_main_quit();
      gdk_threads_leave();
      return NULL;
    }
  }
}

void icon_set()
{
  gtk_status_icon_set_from_pixbuf(status_icon, icon_alert);
}

void icon_reset()
{
  gtk_status_icon_set_from_pixbuf(status_icon, icon_idle);
}

void icon_init()
{
  icon_idle  = gdk_pixbuf_new_from_inline(-1, irssi,       FALSE, NULL);
  icon_alert = gdk_pixbuf_new_from_inline(-1, irssi_alert, FALSE, NULL);

  status_icon = gtk_status_icon_new_from_pixbuf(icon_idle);

  g_signal_connect(G_OBJECT(status_icon), "activate",
                   G_CALLBACK(icon_reset), NULL);
}

void parent(FILE *fnotify)
{
  char buf[1024];
  char *p, *header, *message;
  NotifyNotification *notify;

  while (fgets(buf, sizeof(buf), fnotify))
  {
    p = strchr(buf, ' ');
    if (p == NULL)
    {
      fprintf(stderr, "Invalid data received: no space in input\n");
      continue;
    }

    /* Separate the two strings */
    *p = '\0';

    header = buf;
    message = p + 1;

    if (*header == '\0')
    {
      fprintf(stderr, "Invalid data received: empty header\n");
      continue;
    }

    if (*message == '\0')
    {
      fprintf(stderr, "Warning: empty message\n");
      message = NULL;
    }

    gdk_threads_enter();

    icon_set();

    notify = notify_notification_new(header, message,
                                     "notification-message-im", NULL);
    notify_notification_show(notify, NULL);
    g_object_unref(G_OBJECT(notify));

    gdk_threads_leave();
  }

  fprintf(stderr, "fgets returned NULL\n");

  notify_uninit();

  gdk_threads_enter();
  gtk_main_quit();
  gdk_threads_leave();

  return;
}

int main(int argc, char **argv)
{
  FILE *fnotify;
  int pipefds[2];
  pid_t cpid;
  pthread_t thread;
  int r;

  g_thread_init(NULL);
  gdk_threads_init();

  gdk_threads_enter();
  gtk_init(&argc, &argv);
  gdk_threads_leave();

  if (argc != 2)
  {
    if (argc >= 1)
    {
      fprintf(stderr, "Usage: %s server\n", argv[0]);
    }
    else
    {
      fprintf(stderr, "Usage: ssh_irssi_notify server\n");
    }


    fprintf(stderr, "If connecting via ssh to server requires some other \n"
                    "options, specify these in your ssh_config.\n\n");
    exit(EXIT_FAILURE);
  }

  icon_init();

  if (!notify_init("ssh_irssi_fnotify"))
  {
    fprintf(stderr, "Failed to connect to the notify server");
    exit(EXIT_FAILURE);
  }

  r = pipe(pipefds);
  if (r == -1)
  {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  fnotify = fdopen(pipefds[0], "r");
  if (fnotify == NULL)
  {
    perror("fdopen");
    exit(EXIT_FAILURE);
  }

  cpid = fork();
  if (cpid == -1)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (cpid == 0)
  {
    /* ssh -tt forces sshd to give us a pseudo-tty (pts), meaning that when
     * the connection dies or ends tail will receive a SIGHUP, preventing 
     * a massive amount of useless tail processes */
    char *ssh_exec[] = { "ssh", argv[1], "-tt",
                         "tail -n 0 -f .irssi/fnotify", 
                         NULL };

    close(0);
    dup2(pipefds[1], 1);
    execvp(ssh_exec[0], ssh_exec);
    exit(EXIT_FAILURE);
  }
  else
  {
    if (pthread_create(&thread, NULL, gtk_thread, NULL) != 0)
    {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }

    if (pthread_create(&thread, NULL, reaper_thread, &cpid) != 0)
    {
      perror("pthread_create");
      gdk_threads_enter();
      gtk_main_quit();
      gdk_threads_leave();
      return EXIT_FAILURE;
    }

    parent(fnotify);
    return EXIT_FAILURE;
  }
}
