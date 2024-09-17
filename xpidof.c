/*

Copyright 1990, 1998  The Open Group
Copyright (c) 2000  The XFree86 Project, Inc.

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.
  
The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

Copyright 2007 Kim woelders

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and
that the name of the copyright holders not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.  The copyright holders make no representations
about the suitability of this software for any purpose.  It is provided "as
is" without express or implied warranty.

THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
OF THIS SOFTWARE.

*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>

#include <X11/Xmu/WinUtil.h>

#define SelectButtonAny (-1)
#define SelectButtonFirst (-2)

static char *programName;

static void _X_NORETURN Exit(int code, Display *dpy) {
	if (dpy) {
		XCloseDisplay(dpy);
	}
	exit(code);
}

static int parse_button(const char *s, int *buttonp) {
	if (strcasecmp (s, "any") == 0) {
		*buttonp = SelectButtonAny;
		return (1);
	}

    /* check for non-numeric input */
    for (const char *cp = s; *cp; cp++) {
		if (!(isascii (*cp) && isdigit (*cp))) return (0);  /* bogus name */
    }

    *buttonp = atoi (s);
    return (1);
}

/* Return True if the property WM_STATE is set on the window, otherwise
 * return False.
 */
static Bool wm_state_set(Display *dpy, Window win) {
    Atom wm_state;
    Atom actual_type;
    int success;
    int actual_format;
    unsigned long nitems, remaining;
    unsigned char* prop = NULL;

    wm_state = XInternAtom(dpy, "WM_STATE", True);
    if (wm_state == None) return False;
    success = XGetWindowProperty(dpy, win, wm_state, 0L, 0L, False, 
				 AnyPropertyType, &actual_type, &actual_format,
				 &nitems, &remaining, &prop);
    if (prop) XFree((char *) prop);
    return (success == Success && actual_type != None && actual_format);
}

/* Using a heuristic method, return True if a window manager is running,
 * otherwise, return False.
 */
static Bool wm_running(Display *dpy, int screenno) {
    XWindowAttributes	xwa;
    Status		status;

    status = XGetWindowAttributes(dpy, RootWindow(dpy, screenno), &xwa);
    return (status &&
	    ((xwa.all_event_masks & SubstructureRedirectMask) ||
	     (xwa.all_event_masks & SubstructureNotifyMask)));
}

static XID get_window_id(Display *dpy, int screen, int button, const char *msg) {
    Cursor cursor;		/* cursor to use when selecting */
    Window root;		/* the current root */
    Window retwin = None;	/* the window that got selected */
    int retbutton = -1;		/* button used to select window */
    int pressed = 0;		/* count of number of buttons pressed */

#define MASK (ButtonPressMask | ButtonReleaseMask)

    root = RootWindow (dpy, screen);
    cursor = XCreateFontCursor (dpy, XC_crosshair);
    if (cursor == None) {
	fprintf (stderr, "%s:  unable to create selection cursor\n",
		 programName);
	Exit (1, dpy);
    }

    XSync (dpy, 0);			/* give xterm a chance */

    if (XGrabPointer (dpy, root, False, MASK, GrabModeSync, GrabModeAsync, 
    		      None, cursor, CurrentTime) != GrabSuccess) {
	fprintf (stderr, "%s:  unable to grab cursor\n", programName);
	Exit (1, dpy);
    }

    /* from dsimple.c in xwininfo */
    while (retwin == None || pressed != 0) {
	XEvent event;

	XAllowEvents (dpy, SyncPointer, CurrentTime);
	XWindowEvent (dpy, root, MASK, &event);
	switch (event.type) {
	  case ButtonPress:
	    if (retwin == None) {
		retbutton = event.xbutton.button;
		retwin = ((event.xbutton.subwindow != None) ?
			  event.xbutton.subwindow : root);
	    }
	    pressed++;
	    continue;
	  case ButtonRelease:
	    if (pressed > 0) pressed--;
	    continue;
	}					/* end switch */
    }						/* end for */

    XUngrabPointer (dpy, CurrentTime);
    XFreeCursor (dpy, cursor);
    XSync (dpy, 0);

    return ((button == -1 || retbutton == button) ? retwin : None);
}

int main(int argc, char **argv) {
	Display *dpy = NULL;
	char *displayName = NULL;
	int screenNo;
	XID id = None;
	char *buttonName = NULL;
	int button;
	Bool top = False;

	programName = argv[0];
	button = SelectButtonFirst;

	dpy = XOpenDisplay(displayName);
	if (!dpy) {
		fprintf(stderr, "%s: unable to open display \"%s\"\n", programName, XDisplayName(displayName));
		Exit(1, dpy);
	}

	screenNo = DefaultScreen(dpy);
	if (!buttonName) {
		buttonName = XGetDefault(dpy, programName, "Button");
	}

	if (buttonName && !parse_button(buttonName, &button)) {
		fprintf(stderr, "%s: invalid button specification \"%s\"\n", programName, buttonName);
		Exit(1, dpy);
	}

	if (button >= 0 || button == SelectButtonFirst) {
		unsigned char pointerMap[256]; /* 8 bits of pointer num */
		int count;
		unsigned int ub = (unsigned int) button;

		count = XGetPointerMapping(dpy, pointerMap, 256);
		if (count <= 0) {
			fprintf(stderr, "%s: no pointer mapping, can't select window\n", programName);
			Exit(1, dpy);
		}

		if (button >= 0) {
			int j;
			for (j = 0; j < count; j++) {
				if (ub == (unsigned int)pointerMap[j]) break;
			}

			if (j == count) {
				fprintf(stderr, "%s: no button number %u in pointer map, can't select window\n", programName, ub);
				Exit(1, dpy);
			}
		} else {
			button = (int) ((unsigned int) pointerMap[0]);
		}

		if ((id = get_window_id(dpy, screenNo, button, "the window"))) {
			if (id == RootWindow(dpy, screenNo)) {
				id = None;
			} else if (!top) {
				XID indicated = id;
				if ((id = XmuClientWindow(dpy, indicated)) == indicated) {
					if (!wm_state_set(dpy, id) && wm_running(dpy, screenNo)) {
						id = None;
					}
				}
			}
		}
	}

	if (id != None) {
		XSync(dpy, 0);

		Atom actual_type;
		int actual_format;
		unsigned long nitems;
		unsigned long nbytes;
		unsigned long bytes_after;
		static unsigned char *prop = NULL;

		int max_len = 500000;
		Atom atom = XInternAtom(dpy, "_NET_WM_PID", True);
		if (atom == None) {
			fprintf(stderr, "%s: Failed to find window property _NET_WM_PID\n", programName);
			Exit(1, dpy);
		}
		XGetWindowProperty(dpy, (Window)id, atom, 0, (max_len+3)/4, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop);

		// Cursed
		int actualPID = prop[0] | (prop[1] << 8) | (prop[2] << 16);
		printf("%d\n", actualPID);
	}

	Exit(0, dpy);
	return 0;
}
