#define UBB_SINGLE_REPLACE(to) \
	ubb_single_replace_fn, ubb_single_replace_new(to), ubb_single_replace_free

typedef struct ubb_single_replace_s {
	char			*to;
	int				to_len;
} ubb_single_replace_t;

void * ubb_single_replace_new(const char *to) {
	ubb_single_replace_t *d = (ubb_single_replace_t*)malloc(sizeof(ubb_single_replace_t));
	
	d->to = strdup(to);
	d->to_len = strlen(to);
	
	return (void*)d;
}

void ubb_single_replace_free(void *ptr) {
	ubb_single_replace_t *d = (ubb_single_replace_t*)ptr;
	free(d->to);
	free(d);
}

int ubb_single_replace_fn(ubb_t *ubb, char *data, int data_len, int *matches, int nmatches, char **out, int *out_alloc, void *param, void *gparam) {
	ubb_single_replace_t *d = (ubb_single_replace_t*)param;
	*out = d->to;
	return d->to_len;
}
