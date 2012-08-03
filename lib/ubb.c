//apt-get install libbsd-dev uthash-dev libfcgi-dev libxcrypt-dev
//wget http://downloads.sourceforge.net/uthash/uthash-1.9.5.tar.bz2
//gcc ubb.c -Wall -lbsd -lxcrypt -lpcre -o ubb && ./ubb

#include "ubb.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "tools.h"

#define PCRE_MAX_MATCHES 1024

#define UBB_MAX_CHILDS 1024
#define UBB_REALLOC(ptr, alloc, len, diff) \
	if((alloc) < ((len) + (diff) + 128)*2) { \
		(alloc) = ((len) + (diff) + 128)*3; \
		(ptr) = (char*)realloc(ptr, alloc); \
	}
#define UBB_MALLOC_TRASH(ptr, alloc, len) \
	if(!(ptr) || (alloc) < (len + 128)*2) { \
		if(ptr) free(ptr); \
		(alloc) = (len + 128)*3; \
		(ptr) = (char*)malloc(alloc); \
	}

//#define DEBUG


inline ubb_group_t * ubb_group_inc(ubb_t *ubb, const char *name);
inline void ubb_group_dec(ubb_t *ubb, const char *name);
inline void ubb_group_dec_ptr(ubb_t *ubb, ubb_group_t *gr);

inline void ubb_filter_clean(ubb_filter_t **fl);
inline int ubb_filter_check(ubb_block_t **filters, const int filters_num, ubb_block_t *bl);
inline int ubb_filter_check_group(ubb_block_t **filters, const int filters_num, ubb_group_t *gr);


ubb_t * ubb_init(const int work_buffer_alloc) {
	ubb_t *ubb;
	int erroffset;
	const char *error;
	
	ubb = (ubb_t*)malloc(sizeof(ubb_t));
	
	ubb->re = pcre_compile("\\[([^\\]=]+)(\\s*=\\s*('\\s*([^\\]]*?)\\s*'|&quot;\\s*([^\\]]*?)\\s*&quot;|([^\\]]*?))\\s*)?\\]", PCRE_MULTILINE, &error, &erroffset, 0);
	if(!ubb->re) {
		free(ubb);
		return NULL;
	}
	
	if(work_buffer_alloc) {
		ubb->work_buffer_alloc = work_buffer_alloc;
		ubb->work_buffer = (char*)malloc(sizeof(char)*work_buffer_alloc);
	}
	else ubb->work_buffer = NULL;
	
	ubb->groups = NULL;
	
	ubb->singles = NULL;
	ubb->singles_num = 0;
	ubb->singles_work = malloc(sizeof(ubb_single_t*)*1);
	*ubb->singles_work = NULL;
	ubb->singles_work_alloc = 0;
	
	ubb->blocks = NULL;
	
	ubb->filter_rebuild = 1;
	
	return ubb;
}
void ubb_free(ubb_t *ubb) {
	ubb_group_t *gr, *gr_tmp;
	ubb_single_t *si, *si_tmp;
	ubb_block_t *bl, *bl_tmp;
	pcre_free(ubb->re);
	HASH_ITER(hh, ubb->groups, gr, gr_tmp) ubb_group_dec_ptr(ubb, gr);
	HASH_ITER(hh, ubb->singles, si, si_tmp) ubb_single_del_ptr(ubb, si);
	HASH_ITER(hh, ubb->blocks, bl, bl_tmp) ubb_block_del_ptr(ubb, bl);
	if(ubb->work_buffer) free(ubb->work_buffer);
	if(ubb->singles_work) free(ubb->singles_work);
	free(ubb);
}



inline ubb_group_t * ubb_group_inc(ubb_t *ubb, const char *name) {
	int len;
	ubb_group_t *gr;
	
	len = strlen(name);
	HASH_FIND(hh, ubb->groups, name, len, gr);
	if(!gr) {
		gr = malloc(sizeof(ubb_group_t));
		gr->name = strdup(name);
		gr->num = 1;
		HASH_ADD_KEYPTR(hh, ubb->groups, gr->name, len, gr);
		ubb->filter_rebuild = 1;
	}
	else ++gr->num;
	
	return gr;
}
inline void ubb_group_dec(ubb_t *ubb, const char *name) {
	ubb_group_t *gr;
	HASH_FIND(hh, ubb->groups, name, strlen(name), gr);
	if(gr) ubb_group_dec_ptr(ubb, gr);
}
inline void ubb_group_dec_ptr(ubb_t *ubb, ubb_group_t *gr) {
	if(--gr->num < 0) {
		HASH_DELETE(hh, ubb->groups, gr);
		free(gr->name);
		free(gr);
		ubb->filter_rebuild = 1;
	}
}

void ubb_group_del(ubb_t *ubb, const char *name) {
	ubb_group_t *gr;
	HASH_FIND(hh, ubb->groups, name, strlen(name), gr);
	if(gr) ubb_group_del_ptr(ubb, gr);
}
void ubb_group_del_ptr(ubb_t *ubb, ubb_group_t *gr) {
	ubb_block_t *bl, *bl_tmp;
	ubb_single_t *si, *si_tmp;
	HASH_ITER(hh, ubb->blocks, bl, bl_tmp) if(bl->group == gr) ubb_block_del_ptr(ubb, bl);
	HASH_ITER(hh, ubb->singles, si, si_tmp) if(si->group == gr) ubb_single_del_ptr(ubb, si);
}



int ubb_single_add(ubb_t *ubb, const char *group, const char *name, const int is_regex, const int priority, ubb_single_cb_t fn, void *param, ubb_free_cb_t param_free) {
	int erroffset;
	const char *error;
	ubb_single_t *si, *si2;
	
	si = malloc(sizeof(ubb_single_t));
	si->name = strdup(name);
	si->name_len = strlen(name);
	if(is_regex) {
		si->name_re = pcre_compile(name, PCRE_MULTILINE, &error, &erroffset, 0);
		if(!si->name_re) {
			free(si->name);
			free(si);
			return -1;
		}
	}
	else si->name_re = NULL;
	
	si->priority = priority;
	
	si->fn = fn;
	si->param = param;
	si->param_free = param_free;
	
	si->group = ubb_group_inc(ubb, group);
	
	if(++ubb->singles_num > ubb->singles_work_alloc) {
		if(ubb->singles_work) free(ubb->singles_work);
		ubb->singles_work_alloc = ubb->singles_num;
		ubb->singles_work = (ubb_single_t**)malloc(sizeof(ubb_single_t*)*(ubb->singles_work_alloc + 1));
	}
	
	HASH_FIND(hh, ubb->singles, name, si->name_len, si2);
	if(si2) ubb_single_del_ptr(ubb, si2);
	
	HASH_ADD_KEYPTR(hh, ubb->singles, si->name, si->name_len, si);
	ubb->filter_rebuild = 1;
	return 0;
}
void ubb_single_del(ubb_t *ubb, const char *name) {
	ubb_single_t *si;
	HASH_FIND(hh, ubb->singles, name, strlen(name), si);
	if(si) ubb_single_del_ptr(ubb, si);
}
void ubb_single_del_ptr(ubb_t *ubb, ubb_single_t *si) {
	HASH_DELETE(hh, ubb->singles, si);
	free(si->name);
	if(si->name_re) pcre_free(si->name_re);
	if(si->param && si->param_free) si->param_free(si->param);
	--ubb->singles_num;
	ubb_group_dec_ptr(ubb, si->group);
	free(si);
	ubb->filter_rebuild = 1;
}


int ubb_block_add(ubb_t *ubb, const char *group, const char *name, const char *filter, ubb_block_cb_t fn, void *param, ubb_free_cb_t param_free) {
	ubb_block_t *bl, *bl2;
	
	bl = malloc(sizeof(ubb_block_t));
	bl->name = strdup(name);
	
	bl->filter = NULL;
	bl->filter_str = strdup(filter);
	
	bl->fn = fn;
	bl->param = param ? param : (void*)bl;
	bl->param_free = param_free;
	
	bl->group = ubb_group_inc(ubb, group);
	
	HASH_FIND(hh, ubb->blocks, name, strlen(name), bl2);
	if(bl2) ubb_block_del_ptr(ubb, bl2);
	
	HASH_ADD_KEYPTR(hh, ubb->blocks, bl->name, strlen(bl->name), bl);
	ubb->filter_rebuild = 1;
	return 0;
}
void ubb_block_del(ubb_t *ubb, const char *name) {
	ubb_block_t *bl;
	HASH_FIND(hh, ubb->blocks, name, strlen(name), bl);
	if(bl) ubb_block_del_ptr(ubb, bl);
}
void ubb_block_del_ptr(ubb_t *ubb, ubb_block_t *bl) {
	HASH_DELETE(hh, ubb->blocks, bl);
	free(bl->name);
	free(bl->filter_str);
	if(bl->param && bl->param_free) bl->param_free(bl->param);
	if(bl->filter) ubb_filter_clean(&bl->filter);
	ubb_group_dec_ptr(ubb, bl->group);
	free(bl);
	ubb->filter_rebuild = 1;
}



inline void ubb_filter_rebuild(ubb_t *ubb) {
	char *p, *q;
	int i, found_allow, found_deny, deny_all;
	int allow_groups_num;
	int allow_blocks_num;
	int deny_groups_num;
	int deny_blocks_num;
	ubb_group_t *fgr;
	ubb_block_t *fbl, *bl, *bl_tmp;
	ubb_filter_t *f;
	
	f = (ubb_filter_t*)malloc(sizeof(ubb_filter_t));
	
	f->allow = (ubb_filter_subs_t*)malloc(sizeof(ubb_filter_subs_t));
	f->allow->groups = (ubb_group_t**)malloc(sizeof(ubb_group_t*)*(HASH_CNT(hh, ubb->groups) + 1));
	f->allow->blocks = (ubb_block_t**)malloc(sizeof(ubb_block_t*)*(HASH_CNT(hh, ubb->blocks) + 1));
	
	f->deny = (ubb_filter_subs_t*)malloc(sizeof(ubb_filter_subs_t));
	f->deny->groups = (ubb_group_t**)malloc(sizeof(ubb_group_t*)*(HASH_CNT(hh, ubb->groups) + 1));
	f->deny->blocks = (ubb_block_t**)malloc(sizeof(ubb_block_t*)*(HASH_CNT(hh, ubb->blocks) + 1));
	
	HASH_ITER(hh, ubb->blocks, bl, bl_tmp) {
		ubb_filter_clean(&bl->filter);
		if(!*bl->filter_str) continue;
		
		f->deny_all = 0;
		allow_groups_num = 0;
		allow_blocks_num = 0;
		deny_groups_num = 0;
		deny_blocks_num = 0;
		
		p = bl->filter_str;
		while(p && *p) {
			q = strchr(p, '|');
			if(q) *q++ = 0;
			
			if(*p == '!') {
				deny_all = 0;
				p++;
			}
			else deny_all = 1;
			
			if(*p == '*') {
				f->deny_all = deny_all;
				p = q;
				continue;
			}
			if(*p == '#') {
				p++;
				HASH_FIND(hh, ubb->groups, p, strlen(p), fgr);
				if(!fgr) {
					p = q;
					continue;
				}
				for(i = 0, found_allow = 0; i < allow_groups_num && !found_allow; i++)
					if(f->allow->groups[i] == fgr)
						found_allow = 1;
				for(i = 0, found_deny = 0; i < deny_groups_num && !found_deny; i++)
					if(f->deny->groups[i] == fgr)
						found_deny = 1;
				if(found_allow || found_deny) {
					p = q;
					continue;
				}
				if(deny_all) f->deny->groups[deny_groups_num++] = fgr;
				else f->allow->groups[allow_groups_num++] = fgr;
			}
			else {
				HASH_FIND(hh, ubb->blocks, p, strlen(p), fbl);
				if(!fbl) {
					p = q;
					continue;
				}
				for(i = 0, found_allow = 0; i < allow_blocks_num && !found_allow; i++)
					if(f->allow->blocks[i] == fbl)
						found_allow = 1;
				for(i = 0, found_deny = 0; i < deny_blocks_num && !found_deny; i++)
					if(f->deny->blocks[i] == fbl)
						found_deny = 1;
				if(found_allow || found_deny) {
					p = q;
					continue;
				}
				if(deny_all) f->deny->blocks[deny_blocks_num++] = fbl;
				else f->allow->blocks[allow_blocks_num++] = fbl;
			}
			
			p = q;
		}
		
		if(f->deny_all || allow_groups_num || allow_blocks_num || deny_groups_num || deny_blocks_num) {
			bl->filter = (ubb_filter_t*)malloc(sizeof(ubb_filter_t));
			bl->filter->deny_all = f->deny_all;
			
			if(allow_groups_num || allow_blocks_num) {
				bl->filter->allow = (ubb_filter_subs_t*)malloc(sizeof(ubb_filter_subs_t));
				if(allow_groups_num) {
					bl->filter->allow->groups = (ubb_group_t**)malloc(sizeof(ubb_group_t*)*(allow_groups_num + 1));
					memcpy(bl->filter->allow->groups, f->allow->groups, sizeof(ubb_group_t*)*allow_groups_num);
					bl->filter->allow->groups[allow_groups_num] = NULL;
				}
				else bl->filter->allow->groups = NULL;
				if(allow_blocks_num) {
					bl->filter->allow->blocks = (ubb_block_t**)malloc(sizeof(ubb_block_t*)*(allow_blocks_num + 1));
					memcpy(bl->filter->allow->blocks, f->allow->blocks, sizeof(ubb_block_t*)*allow_blocks_num);
					bl->filter->allow->blocks[allow_blocks_num] = NULL;
				}
				else bl->filter->allow->blocks = NULL;
			}
			else bl->filter->allow = NULL;
			
			if(deny_groups_num || deny_blocks_num) {
				bl->filter->deny = (ubb_filter_subs_t*)malloc(sizeof(ubb_filter_subs_t));
				if(deny_groups_num) {
					bl->filter->deny->groups = (ubb_group_t**)malloc(sizeof(ubb_group_t*)*(deny_groups_num + 1));
					memcpy(bl->filter->deny->groups, f->deny->groups, sizeof(ubb_group_t*)*deny_groups_num);
					bl->filter->deny->groups[deny_groups_num] = NULL;
				}
				else bl->filter->deny->groups = NULL;
				if(deny_blocks_num) {
					bl->filter->deny->blocks = (ubb_block_t**)malloc(sizeof(ubb_block_t*)*(deny_blocks_num + 1));
					memcpy(bl->filter->deny->blocks, f->deny->blocks, sizeof(ubb_block_t*)*deny_blocks_num);
					bl->filter->deny->blocks[deny_blocks_num] = NULL;
				}
				else bl->filter->deny->blocks = NULL;
			}
			else bl->filter->deny = NULL;
		}
	}
	
	ubb_filter_clean(&f);
}
inline void ubb_filter_clean(ubb_filter_t **fl) {
	if(*fl) {
		if((*fl)->allow) {
			if((*fl)->allow->groups) free((*fl)->allow->groups);
			if((*fl)->allow->blocks) free((*fl)->allow->blocks);
			free((*fl)->allow);
		}
		if((*fl)->deny) {
			if((*fl)->deny->groups) free((*fl)->deny->groups);
			if((*fl)->deny->blocks) free((*fl)->deny->blocks);
			free((*fl)->deny);
		}
		free(*fl);
		*fl = NULL;
	}
}

inline int ubb_filter_check(ubb_block_t **filters, int filters_num, ubb_block_t *bl) {
	int i, deny_tag;
	ubb_group_t **fgr;
	ubb_block_t **fbl;
	for(i = 0, deny_tag = 0; i < filters_num && !deny_tag; i++) {
		if(filters[i]->filter->deny_all) deny_tag = 1;
		else if(filters[i]->filter->deny) {
			if(filters[i]->filter->deny->groups)
				for(fgr = filters[i]->filter->deny->groups; *fgr && !deny_tag; fgr++)
					if(*fgr == bl->group)
						deny_tag = 1;
			if(!deny_tag && filters[i]->filter->deny->blocks)
				for(fbl = filters[i]->filter->deny->blocks; *fbl && !deny_tag; fbl++)
					if(*fbl == bl)
						deny_tag = 1;
		}
		if(deny_tag && filters[i]->filter->allow) {
			if(filters[i]->filter->allow->groups)
				for(fgr = filters[i]->filter->allow->groups; *fgr && deny_tag; fgr++)
					if(*fgr == bl->group)
						deny_tag = 0;
			if(deny_tag && filters[i]->filter->allow->blocks)
				for(fbl = filters[i]->filter->allow->blocks; *fbl && deny_tag; fbl++)
					if(*fbl == bl)
						deny_tag = 0;
		}
	}
	return deny_tag;
}
inline int ubb_filter_check_group(ubb_block_t **filters, int filters_num, ubb_group_t *gr) {
	int i, deny_tag;
	ubb_group_t **fgr;
	for(i = 0, deny_tag = 0; i < filters_num && !deny_tag; i++) {
		if(filters[i]->filter->deny_all) deny_tag = 1;
		else if(filters[i]->filter->deny) {
			if(filters[i]->filter->deny->groups)
				for(fgr = filters[i]->filter->deny->groups; *fgr && !deny_tag; fgr++)
					if(*fgr == gr)
						deny_tag = 1;
		}
		if(deny_tag && filters[i]->filter->allow) {
			if(filters[i]->filter->allow->groups)
				for(fgr = filters[i]->filter->allow->groups; *fgr && deny_tag; fgr++)
					if(*fgr == gr)
						deny_tag = 0;
		}
	}
	return deny_tag;
}













typedef struct ubb_tag_s {
	ubb_block_t *bl;
	int all_beg;
	int all_end;
	int tag_beg;
	int tag_end;
	int arg_beg;
	int arg_end;
} ubb_tag_t;


inline int ubb_single_parse(ubb_t *ubb, char **data, int *len, int *alloc, ubb_block_t **filters, int *filters_num, int offset, int *offset_single, void *gparam) {
	int i, data_len = 0, new_len, *new_alloc, matches[PCRE_MAX_MATCHES + 1], nmatches = 0, out[PCRE_MAX_MATCHES + 1];
	char c, *p, *q;
	int diff, total_diff;
	ubb_single_t *next, **si;
	
	memset(matches, 0xff, sizeof(matches));
	
	next = NULL;
	total_diff = 0;
	new_alloc = &ubb->work_buffer_alloc;
	while(*offset_single < offset) {
		new_len = offset - *offset_single;
		p = *data + offset;
		c = *p;
		*p = 0;
		p = *data + *offset_single;
		*offset_single = *len;
		for(si = ubb->singles_work; *si; si++) {
			if(filters_num && ubb_filter_check_group(filters, *filters_num, (*si)->group)) continue;
			if((*si)->name_re) {
				i = pcre_exec((*si)->name_re, 0, p, new_len, 0, 0, out, PCRE_MAX_MATCHES);
				if(i >= 0 && (diff = (p + out[0]) - *data) <= *offset_single && (!next || diff < *offset_single || next->priority <= (*si)->priority)) {
					nmatches = i;
					memcpy(matches, out, sizeof(int)*PCRE_MAX_MATCHES);
					next = *si;
					data_len = matches[1] - matches[0];
					*offset_single = diff;
				}
			}
			else {
				q = strstr(p, (*si)->name);
				if(q && (diff = q - *data) <= *offset_single && (!next || diff < *offset_single || next->priority <= (*si)->priority)) {
					next = *si;
					data_len = (*si)->name_len;
					*offset_single = diff;
					nmatches = -1;
				}
			}
		}
		*(*data + offset) = c;
		
		if(!next) break;
		
		if(*offset_single < offset) {
			p = *data + *offset_single;
			
			UBB_MALLOC_TRASH(ubb->work_buffer, ubb->work_buffer_alloc, *len);
			
			if(next->name_re && (diff = matches[0] - *offset_single) != 0)
				for(i = 0; i <= PCRE_MAX_MATCHES && matches[i] != 0xffffffff; i++)
					if(matches[i] != -1)
						matches[i] -= diff;
			
			c = *(p + data_len);
			*(p + data_len) = 0;
			q = ubb->work_buffer;
			new_len = next->fn(ubb, *data, data_len, matches, nmatches, &q, new_alloc, next->param, gparam);
			*(p + data_len) = c;
			
			diff = new_len - data_len;
			if(diff) {
				UBB_REALLOC(*data, *alloc, *len, diff);
				p = *data + *offset_single;
				memmove(p + new_len, p + data_len, *len - (*offset_single + data_len));
				*len += diff;
				offset += diff;
				total_diff += diff;
			}
			memcpy(p, q, new_len);
			
			*offset_single += new_len;
		}
	}
	return total_diff;
}

inline int ubb_block_parse(ubb_t *ubb, char **data, int *len, int *alloc, ubb_tag_t *parents, int *parents_num, ubb_block_t **filters, int *filters_num, int begin, int *end, ubb_block_t *bl, void *gparam) {
	char c, *p, *out;
	ubb_tag_t *tag;
	int out_len, *out_alloc, diff, total_diff = 0;
	
	UBB_MALLOC_TRASH(ubb->work_buffer, ubb->work_buffer_alloc, *len);
	
	out_alloc = &ubb->work_buffer_alloc;
	do {
		if(*parents_num == 0) break;
		tag = &parents[--*parents_num];
		
		if(tag->bl->filter && tag->bl == filters[*filters_num - 1]) --*filters_num;
		
		out_len = begin - tag->all_end;
		
		p = *data + tag->all_end + out_len;
		c = *p;
		*p = 0;
		if(tag->arg_beg != -1) *(*data + tag->arg_end) = 0;
		
		out = ubb->work_buffer;
		out_len = tag->bl->fn(
			ubb,
			*data + tag->all_end, out_len,
			tag->arg_beg == -1 ? NULL : *data + tag->arg_beg, tag->arg_end == -1 ? 0 : tag->arg_end - tag->arg_beg,
			&out, out_alloc, tag->bl->param, gparam);
		
		*p = c;
		if(out_len == -1) continue;
		
		diff = out_len - (*end - tag->all_beg);
		if(diff) {
			UBB_REALLOC(*data, *alloc, *len, diff);
			memmove(*data + tag->all_beg + out_len, *data + *end, *len - *end);
			total_diff += diff;
			*len += diff;
			*end += diff;
		}
		begin = *end;
		if(out_len) memcpy(*data + tag->all_beg, out, out_len);
	}
	while(parents_num && tag->bl != bl);
	return total_diff;
}

int ubb_parse(ubb_t *ubb, char **data, int len, int *alloc, void *gparam) {
	int 			i, rc, denied;
	int 			total_diff = 0;
	char 			*p;
	int 			out[PCRE_MAX_MATCHES + 1];
	int 			offset = 0;
	int 			offset_single = 0;
	int 			is_closing_tag;
	
	ubb_tag_t		parents[UBB_MAX_CHILDS];
	int				parents_num = 0;
	
	ubb_block_t		*filters[UBB_MAX_CHILDS];
	int				filters_num = 0;
	
	ubb_block_t		*bl;
	ubb_single_t	**si_ptr, *si, *si_tmp;
	
	if(ubb->filter_rebuild) {
		ubb_filter_rebuild(ubb);
		ubb->filter_rebuild = 0;
	}
	
	if(ubb->singles_num) {
		si_ptr = ubb->singles_work;
		HASH_ITER(hh, ubb->singles, si, si_tmp) {
			if((si->name_re && pcre_exec(si->name_re, 0, *data, len, 0, 0, out, PCRE_MAX_MATCHES) >= 0) || strstr(*data, si->name) != NULL)
				*si_ptr++ = si;
		}
		*si_ptr = NULL;
	}
	else *ubb->singles_work = NULL;
	
	len = htmlspecialchars2(*data, len);
	len = str_replace("\r", 1, "", 0, *data, len);
	len = str_replace("\t", 1, "    ", 4, *data, len);
	
	while((rc = pcre_exec(ubb->re, 0, *data, len, offset, 0, out, PCRE_MAX_MATCHES)) >= 0) {
		offset = out[1];
		
		p = *data + out[2];
		i = out[3] - out[2];
		
		if(*p == '/') {
			is_closing_tag = 1;
			p++;
			i--;
		}
		else is_closing_tag = 0;
		
		strtolower2(p, i);
		
		HASH_FIND(hh, ubb->blocks, p, i, bl);
		if(!bl) continue;
		
		if(*ubb->singles_work) {
			total_diff = ubb_single_parse(ubb, data, &len, alloc, filters, &filters_num, out[0], &offset_single, gparam);
			if(total_diff) {
				offset += total_diff;
				out[0] += total_diff;
				out[1] += total_diff;
				out[2] += total_diff;
				out[3] += total_diff;
			}
		}
		else total_diff = 0;
		offset_single = out[1];
		
		denied = filters_num ? ubb_filter_check(filters, filters_num, bl) : 0;
		
		if(is_closing_tag) {
			if(denied && bl != parents[parents_num - 1].bl) continue;
			for(i = parents_num - 1; i >= 0; i--) {
				if(parents[i].bl == bl) {
					offset_single += ubb_block_parse(
						ubb, data, &len, alloc, parents, &parents_num, filters, &filters_num, out[0], &offset, bl, gparam);
					break;
				}
			}
		}
		else {
			if(denied) continue;
			parents[parents_num].bl = bl;
			parents[parents_num].all_beg = out[0];
			parents[parents_num].all_end = out[1];
			parents[parents_num].tag_beg = out[2];
			parents[parents_num].tag_end = out[3];
			if(rc < 5) {
				parents[parents_num].arg_beg = -1;
				parents[parents_num].arg_end = -1;
			} else {
				i = (rc - 1)*2;
				parents[parents_num].arg_beg = out[i + 0] + (out[i] == -1 ? 0 : total_diff);
				parents[parents_num].arg_end = out[i + 1] + (out[i] == -1 ? 0 : total_diff);
			}
			if(bl->filter) filters[filters_num++] = bl;
			parents_num++;
		}
	}
	
	while(len > 0 && (*(*data + len - 1) == '\n' || *(*data + len - 1) == ' ')) len--;
	*(*data + len) = 0;
	
	if(*ubb->singles_work)
		ubb_single_parse(ubb, data, &len, alloc, filters, &filters_num, len, &offset_single, gparam);
	
	if(parents_num) {
		offset = len;
		ubb_block_parse(ubb, data, &len, alloc, parents, &parents_num, filters, &filters_num, len, &offset, NULL, gparam);
	}
	
	UBB_REALLOC(*data, *alloc, len, len);
	
	*(*data + len) = 0;
	len = str_replace("\n", 1, "<br>", 4, *data, len);
	len = str_replace("[/*]", 4, "", 0, *data, len);
	
	return len;
}







#ifdef DEBUG

#include "ubb_block_snippet.c"
#include "ubb_block_template.c"
#include "ubb_single_replace.c"
#include "ubb_single_replace_regex.c"


int ubb_tag_list(ubb_t *ubb, char *data, int data_len, char *arg, int arg_len, char *out, void *param) {
	char *p = out;
	if(arg) {
		p = strmov(p, "<ol type=\"");
		p = strmov(p, arg);
		p = strmov(p, "\">");
		p = strmov(p, data);
		p = strmov(p, "</ol>");
	}
	else {
		p = strmov(p, "<ul>");
		p = strmov(p, data);
		p = strmov(p, "</ul>");
	}
	return p - out;
}


int main() {
	int i;
	FILE *f;
	ubb_t *ubb;
	char *buffer;
	char *out;
	int len, alloc;
	
	alloc = 8192;
	buffer = malloc(alloc);
	
	f = fopen("../test.txt", "r");
	if(!f) return 1;
	len = fread(buffer, 1, alloc, f);
	fclose(f);
	
	ubb = ubb_init(0);
	
	ubb_block_add(ubb, "inline", "b", "", UBB_BLOCK_SNIPPET("<b>", NULL, "</b>", NULL, 0));
	
	ubb_block_add(ubb, "inline", "b", "", UBB_BLOCK_SNIPPET("<b>", NULL, "</b>", NULL, 0));
	ubb_block_add(ubb, "inline", "i", "s|t|i", UBB_BLOCK_SNIPPET("<i>", NULL, "</i>", NULL, 0));
	ubb_block_add(ubb, "inline", "u", "#smiley", UBB_BLOCK_SNIPPET("<u>", NULL, "</u>", NULL, 0));
	ubb_block_add(ubb, "inline", "s", "", UBB_BLOCK_SNIPPET("<s>", NULL, "</s>", NULL, 0));
	ubb_block_add(ubb, "inline", "t", "", UBB_BLOCK_SNIPPET("<t>", NULL, "</t>", NULL, 0));
	ubb_block_add(ubb, "inline", "strike", "", UBB_BLOCK_SNIPPET("<strike>", NULL, "</strike>", NULL, 0));
	ubb_block_add(ubb, "inline", "url", "", UBB_BLOCK_SNIPPET("<a href=\"", "\">", "</a>", NULL, 1));
	
	ubb_block_add(ubb, "inline", "img", "", UBB_BLOCK_TEMPLATE(
		"<div><img src=\"{D}\" /><br><em>{A}</em></div>",
		"<img src=\"{D}\" />",
		NULL, 0));
	
	/*ubb_block_add(ubb, "list", NULL, NULL, NULL, NULL, 0, ubb_tag_list, NULL, NULL, 0);
	ubb_block_add(ubb, "*", "<li>", NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
	
	ubb_block_add(ubb, "font", "<span style=\"font-family:", ";\">", "</span>", NULL, 0, NULL, NULL, NULL, 0);
	ubb_block_add(ubb, "size", "<span style=\"font-size:", "px;\">", "</span>", NULL, 0, NULL, NULL, NULL, 0);
	ubb_block_add(ubb, "color", "<span style=\"color:", ";\">", "</span>", NULL, 0, NULL, NULL, NULL, 0);
	
	ubb_block_add(ubb, "center", "<div class=\"center\">", NULL, "</div>", NULL, 0, NULL, NULL, NULL, 0);
	ubb_block_add(ubb, "left", "<div style=\"text-align:left;\">", NULL, "</div_left>", NULL, 0, NULL, NULL, NULL, 0);
	ubb_block_add(ubb, "right", "<div class=\"right\">", NULL, "</div>", NULL, 0, NULL, NULL, NULL, 0);
	
	ubb_block_add(ubb, "code", "<div class=\"code-header\">", "</div><pre class=\"code-content\">", "</pre>", "Code:", 0, NULL, NULL, NULL, 1);
	ubb_block_add(ubb, "img", "<img src=\"", NULL, "\">", NULL, 1, NULL, NULL, NULL, 1);
	ubb_block_add(ubb, "url", "<a href=\"", "\">", "</a>", NULL, 1, NULL, NULL, NULL, 0);*/
	
	ubb_single_add(ubb, "smiley", ":D", 0, 0, UBB_SINGLE_REPLACE(":grin:"));
	ubb_single_add(ubb, "smiley", ":)", 0, 0, UBB_SINGLE_REPLACE(":smile:"));
	ubb_single_add(ubb, "smiley", ":(", 0, 0, UBB_SINGLE_REPLACE(":sad:"));
	//ubb_single_add(ubb, "inline_url", "(http|ftp)s?://[^\\s<>]+\\.[^\\s<>]+(/[^\\s<>]*)?", 1, 0, UBB_SINGLE_REPLACE_REGEX("<a href=\"\\0\" target=\"_blank\">\\0</a>"));
	//ubb_single_add(ubb, "inline_url", "(http|ftp)s?://fuck\\.me(/[^\\s<>]*)?", 1, 1, UBB_SINGLE_REPLACE_REGEX("<a href=\"\\0\">\\0</a>"));
	
	out = malloc(alloc);
	memcpy(out, buffer, len + 1);
	i = ubb_parse(ubb, &out, len, &alloc, NULL);
	fwrite(out, 1, i, stdout);
	printf("\nlen = %i\n", i);
	
	/*for(i = 0; i < 7000; i++) {
		memcpy(out, buffer, len + 1);
		ubb_parse(ubb, &out, len, &alloc, NULL);
	}*/
	
	ubb_free(ubb);
	free(buffer);
	free(out);
	
	return 0;
}
#endif
