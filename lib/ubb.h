#ifndef _UBB_H_
#define _UBB_H_

#include <uthash.h>
#include <pcre.h>


typedef struct ubb_s				ubb_t;
typedef struct ubb_group_s			ubb_group_t;
typedef struct ubb_filter_s			ubb_filter_t;
typedef struct ubb_filter_subs_s	ubb_filter_subs_t;
typedef struct ubb_single_s			ubb_single_t;
typedef struct ubb_block_s			ubb_block_t;


//int ubb_single_cb(ubb_t *ubb, char *data, int data_len, int *matches, int nmatches, char **out, int *out_alloc, void *param, void *gparam)
typedef int (*ubb_single_cb_t)(ubb_t *ubb, char*, int, int*, int, char**, int*, void*, void*);

//int ubb_block_cb(ubb_t *ubb, char *data, int data_len, char *arg, int arg_len, char **out, int *out_alloc, void *param, void *gparam)
typedef int (*ubb_block_cb_t)(ubb_t *ubb, char*, int, char*, int, char**, int*, void*, void*);

//void ubb_free_cb(void *ptr)
typedef void (*ubb_free_cb_t)(void*);


#define UBB_FILTER_ALL	0
#define UBB_FILTER_YES	1
#define UBB_FILTER_NO	2
// !* = NONE
// *  = ALL
//u|b|i = FILTER u, b, i
//*|!i = FILTER ALL BUT i
//*|!#smileys = FILTER ALL BUT GROUP smileys
//#smileys = FILTER GROUP smileys

struct ubb_filter_s {
	int					deny_all;
	ubb_filter_subs_t	*allow;
	ubb_filter_subs_t	*deny;
};
struct ubb_filter_subs_s {
	ubb_group_t		**groups;
	ubb_block_t		**blocks;
};


struct ubb_group_s {
	UT_hash_handle	hh;
	char			*name;
	int				num;
};

struct ubb_single_s {
	UT_hash_handle	hh;
	
	ubb_group_t		*group;
	char			*name;
	int				name_len;
	pcre			*name_re;
	
	int				priority;
	
	ubb_single_cb_t	fn;
	void			*param;
	ubb_free_cb_t	param_free;
};

struct ubb_block_s {
	UT_hash_handle	hh;
	
	ubb_group_t		*group;
	char			*name;
	
	ubb_filter_t	*filter;
	char			*filter_str;
	
	ubb_block_cb_t	fn;
	void			*param;
	ubb_free_cb_t	param_free;
};

struct ubb_s {
	pcre			*re;
	
	char			*work_buffer;
	int 			work_buffer_alloc;
	
	ubb_group_t		*groups;
	
	ubb_single_t	*singles;
	int				singles_num;
	ubb_single_t	**singles_work;
	int				singles_work_alloc;
	
	ubb_block_t		*blocks;
	
	int				filter_rebuild;
};


ubb_t * ubb_init(const int work_buffer_alloc);
void ubb_free(ubb_t *ubb);

void ubb_group_del(ubb_t *ubb, const char *name);
void ubb_group_del_ptr(ubb_t *ubb, ubb_group_t *gr);

int ubb_single_add(ubb_t *ubb, const char *group, const char *name, const int is_regex, const int priority, ubb_single_cb_t fn, void *param, ubb_free_cb_t param_free);
void ubb_single_del(ubb_t *ubb, const char *name);
void ubb_single_del_ptr(ubb_t *ubb, ubb_single_t *bl);

int ubb_block_add(ubb_t *ubb, const char *group, const char *name, const char *filter, ubb_block_cb_t fn, void *param, ubb_free_cb_t param_free);
void ubb_block_del(ubb_t *ubb, const char *name);
void ubb_block_del_ptr(ubb_t *ubb, ubb_block_t *bl);

int ubb_parse(ubb_t *ubb, char **data, int len, int *alloc, void *gparam);

#endif
