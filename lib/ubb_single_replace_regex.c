#define UBB_SINGLE_REPLACE_REGEX(to) \
	ubb_single_replace_regex_fn, ubb_single_replace_regex_new(to), ubb_single_replace_regex_free

typedef struct ubb_single_replace_regex_s {
	char			*to;
	int				to_len;
	pcre			*re;
} ubb_single_replace_regex_t;

void * ubb_single_replace_regex_new(const char *to) {
	ubb_single_replace_regex_t *d = (ubb_single_replace_regex_t*)malloc(sizeof(ubb_single_replace_regex_t));
	int erroffset;
	const char *error;
	
	d->to = strdup(to);
	d->to_len = strlen(to);
	
	d->re = pcre_compile("\\\\(\\d+)", PCRE_MULTILINE, &error, &erroffset, 0);
	if(!d->re) {
		free(d->to);
		free(d);
		return NULL;
	}
	
	return (void*)d;
}

void ubb_single_replace_regex_free(void *ptr) {
	ubb_single_replace_regex_t *d = (ubb_single_replace_regex_t*)ptr;
	pcre_free(d->re);
	free(d->to);
	free(d);
}

int ubb_single_replace_regex_fn(ubb_t *ubb, char *data, int data_len, int *matches, int nmatches, char **out, int *out_alloc, void *param, void *gparam) {
	ubb_single_replace_regex_t *d = (ubb_single_replace_regex_t*)param;
	int i, diff, out_len = 0, offset = 0, m[PCRE_MAX_MATCHES];
	
	while(pcre_exec(d->re, 0, d->to, d->to_len, offset, 0, m, PCRE_MAX_MATCHES) >= 0) {
		diff = m[0] - offset;
		memcpy(*out + out_len, d->to + offset, diff);
		out_len += diff;
		
		i = atoi(d->to + m[2]);
		if(i >= 0 && i <= offset && matches[i*2 + 1] != -1) {
			diff = matches[i*2 + 1] - matches[i*2];
			memcpy(*out + out_len, data + matches[i*2], diff);
			out_len += diff;
		}
		
		offset = m[1];
	}
	if(offset != d->to_len) {
		diff = d->to_len - offset;
		memcpy(*out + out_len, d->to + offset, diff);
		out_len += diff;
	}
	
	return out_len;
}
