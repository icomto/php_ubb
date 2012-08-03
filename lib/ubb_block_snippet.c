#define UBB_BLOCK_SNIPPET(begin, middle, end, default_arg, arg_fallback_to_data) \
	ubb_block_snippet_fn, ubb_block_snippet_new(begin, middle, end, default_arg, arg_fallback_to_data), ubb_block_snippet_free

typedef struct ubb_block_snippet_s {
	char			*begin;
	int				begin_len;
	char			*middle;
	int				middle_len;
	char			*end;
	int				end_len;
	char			*default_arg;
	int				default_arg_len;
	int				arg_fallback_to_data:1;
} ubb_block_snippet_t;

void * ubb_block_snippet_new(const char *begin, const char *middle, const char *end, const char *default_arg, const int arg_fallback_to_data) {
	ubb_block_snippet_t *d = (ubb_block_snippet_t*)malloc(sizeof(ubb_block_snippet_t));
	
	d->begin = begin ? strdup(begin) : NULL;
	d->begin_len = begin ? strlen(begin) : 0;
	
	d->middle = middle ? strdup(middle) : NULL;
	d->middle_len = middle ? strlen(middle) : 0;
	
	d->end = end ? strdup(end) : NULL;
	d->end_len = end ? strlen(end) : 0;
	
	d->default_arg = default_arg ? strdup(default_arg) : NULL;
	d->default_arg_len = default_arg ? strlen(default_arg) : 0;
	
	d->arg_fallback_to_data = arg_fallback_to_data;
	
	return (void*)d;
}

void ubb_block_snippet_free(void *ptr) {
	ubb_block_snippet_t *d = (ubb_block_snippet_t*)ptr;
	if(d->begin) free(d->begin);
	if(d->middle) free(d->middle);
	if(d->end) free(d->end);
	if(d->default_arg) free(d->default_arg);
	free(d);
}

int ubb_block_snippet_fn(ubb_t *ubb, char *data, int data_len, char *arg, int arg_len, char **out, int *out_alloc, void *param, void *gparam) {
	ubb_block_snippet_t *d = (ubb_block_snippet_t*)param;
	char *p = *out;
	if(d->begin) {
		memcpy(p, d->begin, d->begin_len);
		p += d->begin_len;
	}
	if(d->middle) {
		if(arg_len) {
			memcpy(p, arg, arg_len);
			p += arg_len;
		}
		else if(d->default_arg) {
			memcpy(p, d->default_arg, d->default_arg_len);
			p += d->default_arg_len;
		}
		else if(d->arg_fallback_to_data) {
			memcpy(p, data, data_len);
			p += data_len;
		}
		memcpy(p, d->middle, d->middle_len);
		p += d->middle_len;
	}
	memcpy(p, data, data_len);
	p += data_len;
	if(d->end) {
		memcpy(p, d->end, d->end_len);
		p += d->end_len;
	}
	return p - *out;
}
