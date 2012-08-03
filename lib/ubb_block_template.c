#define UBB_BLOCK_TEMPLATE(without_arg, with_arg, default_arg, use_data_as_default_arg) \
	ubb_block_template_fn, ubb_block_template_new(without_arg, with_arg, default_arg, use_data_as_default_arg), ubb_block_template_free

typedef struct ubb_block_template_element_s {
	char			*data;
	int				len;
} ubb_block_template_element_t;
typedef struct ubb_block_template_s {
	ubb_block_template_element_t	**without_arg;
	char							*without_arg_buf;
	ubb_block_template_element_t	**with_arg;
	char							*with_arg_buf;
	ubb_block_template_element_t	*default_arg;
	int								use_data_as_default_arg;
} ubb_block_template_t;

ubb_block_template_element_t ** ubb_block_template_new_arg(char *buf) {
	int i, type;
	char *p, *q, *r;
	ubb_block_template_element_t **bte, **btep;
	
	for(i = 1, p = buf; ; p = q + 3) {
		q = strstr(p, "{D}");
		r = strstr(p, "{A}");
		if(!q) {
			if(!r) {
				if(*p) ++i;
				break;
			}
			q = r;
		}
		else if(r && r < q) q = r;
		if(q - p) i += 2;
		else ++i;
	}
	
	bte = (ubb_block_template_element_t**)malloc(sizeof(ubb_block_template_element_t*)*i);
	btep = bte;
	for(p = buf; ; p = q + 3) {
		q = strstr(p, "{D}");
		r = strstr(p, "{A}");
		if(!q) {
			if(!r) {
				if(*p) {
					*btep = (ubb_block_template_element_t*)malloc(sizeof(ubb_block_template_element_t));
					(*btep)->data = p;
					(*btep)->len = strlen(p);
					++btep;
				}
				break;
			}
			type = -2;
			q = r;
		}
		else {
			if(r && r < q) {
				type = -2;
				q = r;
			}
			else type = -1;
		}
		
		*q = 0;
		
		if(q - p) {
			*btep = (ubb_block_template_element_t*)malloc(sizeof(ubb_block_template_element_t));
			(*btep)->data = p;
			(*btep)->len = q - p;
			++btep;
		}
		
		*btep = (ubb_block_template_element_t*)malloc(sizeof(ubb_block_template_element_t));
		(*btep)->data = NULL;
		(*btep)->len = type;
		++btep;
	}
	
	*btep = NULL;
	return bte;
}
void * ubb_block_template_new(const char *without_arg, const char *with_arg, const char *default_arg, const int use_data_as_default_arg) {
	ubb_block_template_t *d = (ubb_block_template_t*)malloc(sizeof(ubb_block_template_t));
	
	if(without_arg) {
		d->without_arg_buf = strdup(without_arg);
		d->without_arg = ubb_block_template_new_arg(d->without_arg_buf);
	}
	else d->without_arg = NULL;
	
	if(with_arg) {
		d->with_arg_buf = strdup(with_arg);
		d->with_arg = ubb_block_template_new_arg(d->with_arg_buf);
	}
	else d->with_arg = NULL;
	
	if(default_arg) {
		d->default_arg = (ubb_block_template_element_t*)malloc(sizeof(ubb_block_template_element_t));
		d->default_arg->data = strdup(default_arg);
		d->default_arg->len = strlen(default_arg);
	}
	else d->default_arg = NULL;
	
	d->use_data_as_default_arg = use_data_as_default_arg;
	
	return (void*)d;
}

void ubb_block_template_free(void *ptr) {
	ubb_block_template_t *d = (ubb_block_template_t*)ptr;
	ubb_block_template_element_t **btep;
	if(d->without_arg) {
		for(btep = d->without_arg; *btep; btep++)
			free(*btep);
		free(d->without_arg);
		free(d->without_arg_buf);
	}
	if(d->with_arg) {
		for(btep = d->with_arg; *btep; btep++)
			free(*btep);
		free(d->with_arg);
		free(d->with_arg_buf);
	}
	if(d->default_arg) free(d->default_arg);
	free(d);
}

int ubb_block_template_fn(ubb_t *ubb, char *data, int data_len, char *arg, int arg_len, char **out, int *out_alloc, void *param, void *gparam) {
	ubb_block_template_t *d = (ubb_block_template_t*)param;
	ubb_block_template_element_t **btep = NULL;
	char *p = *out;
	
	if(!arg && d->with_arg) {
		if(d->default_arg) {
			arg = d->default_arg->data;
			arg_len = d->default_arg->len;
		}
		else if(d->use_data_as_default_arg) {
			arg = data;
			arg_len = data_len;
		}
		else if(d->without_arg) {
			btep = d->without_arg;
		}
		else
			return -1;
	}
	if(!btep && !(btep = d->without_arg) && !(btep = d->with_arg))
		return -1;
	
	while(*btep) {
		switch((*btep)->len) {
		case -1:
			memcpy(p, data, data_len);
			p += data_len;
			break;
		case -2:
			memcpy(p, arg, arg_len);
			p += arg_len;
			break;
		default:
			memcpy(p, (*btep)->data, (*btep)->len);
			p += (*btep)->len;
			break;
		}
		++btep;
	}
	
	return p - *out;
}
