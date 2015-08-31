#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

static xcb_connection_t *conn;
static xcb_screen_t *scrn;

void
init_xcb(xcb_connection_t **con)
{
    *con = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(*con)) {
	errx(1, "unable to connect to the X server");
    }
}

void
get_screen(xcb_connection_t *con, xcb_screen_t **scr)
{
    *scr = xcb_setup_roots_iterator(xcb_get_setup(con)).data;
    if (*scr == NULL) {
	errx(1, "unable to retrieve screen information");
    }
}


int
get_windows(xcb_connection_t *con, xcb_window_t w, xcb_window_t **l)
{
    uint32_t childnum = 0;
    xcb_query_tree_cookie_t c;
    xcb_query_tree_reply_t *r;

    c = xcb_query_tree(con, w);
    r = xcb_query_tree_reply(con, c, NULL);
    if (r == NULL) {
	errx(1, "0x%08x: no such window", w);
    }

    *l = malloc(sizeof(xcb_window_t) * r->children_len);
    memcpy(*l, xcb_query_tree_children(r),
	   sizeof(xcb_window_t) * r->children_len);

    childnum = r->children_len;

    free(r);

    return childnum;
}

int
mapped(xcb_connection_t *con, xcb_window_t w)
{
    int ms;
    xcb_get_window_attributes_cookie_t c;
    xcb_get_window_attributes_reply_t *r;

    c = xcb_get_window_attributes(con, w);
    r = xcb_get_window_attributes_reply(con, c, NULL);

    if (r == NULL) {
	return 0;
    }

    ms = r->map_state;

    free(r);

    return ms == XCB_MAP_STATE_VIEWABLE;
}

void
print_window_name(xcb_window_t w)
{
    // Window property variables
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *r;

    cookie = xcb_get_property(conn, 0, w,
			      XCB_ATOM_WM_NAME,
			      XCB_ATOM_STRING,
			      0L, 32L);
    r = xcb_get_property_reply(conn, cookie, NULL);

    if (r) {
	printf("%s\n", (char *) xcb_get_property_value(r));
    }

    free(r);
}

int
ignore(xcb_connection_t *con, xcb_window_t w)
{
	int or;
	xcb_get_window_attributes_cookie_t c;
	xcb_get_window_attributes_reply_t  *r;

	c = xcb_get_window_attributes(con, w);
	r = xcb_get_window_attributes_reply(con, c, NULL);

	if (r == NULL)
		return 0;

	or = r->override_redirect;

	free(r);
	return or;
}

int
main(int argc, char **argv)
{

    init_xcb(&conn);
    get_screen(conn, &scrn);

    int i, wn;
    xcb_window_t *wc;

    wn = get_windows(conn, scrn->root, &wc);

    if (wc == NULL) {
	errx(1, "0x%08x: unable to retreive children", scrn->root);
    }

    for (i = 0; i < wn; i++) {
	if (!ignore(conn, wc[i])) {
	    print_window_name(wc[i]);
	}
    }

    free(wc);
}
