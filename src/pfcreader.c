#include "pfcreader.h"



/* initializer reader */
void pfcR_init(pfc_State *ps, Reader *r, pfc_ReaderFn reader, void *ud)
{
	r->n = 0;
	r->buff = NULL;
	r->reader = reader;
	r->userdata = ud;
	r->ps = ps;
}


/* 
 * Invoke 'pfc_ReaderFn' returning the first character or PFCEOF.
 * 'pfc_ReaderFn' should set the 'size' to the amount of bytes
 * reader read and return the pointer to the start of that
 * buffer. 
 */
int pfcR_fill(Reader *r)
{
	pfc_State *ps;
	size_t size;
	const char *buff;

	ps = r->ps;
	buff = r->reader(ps, r->userdata, &size);
	if (buff == NULL || size == 0)
		return PFCEOF;
	r->buff = buff;
	r->n = size - 1;
	return *r->buff++;
}


/* 
 * Read 'n' bytes from 'Reader' returning
 * count of unread bytes or 0 if all bytes were read. 
 */
size_t pfcR_readn(Reader *r, size_t n)
{
	size_t min;

	while (n) {
		if (r->n == 0) {
			if (pfcR_fill(r) == PFCEOF)
				return n;
			r->n++; /* pfcR_fill decremented it */
			r->buff--; /* restore that character */
		}
		min = (r->n <= n ? r->n : n);
		r->n -= min;
		r->buff += min;
		n -= min;
	}
	return 0;
}
