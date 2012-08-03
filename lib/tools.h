#ifndef _TOOLS_H_
#define _TOOLS_H_

inline char * strmov(register char *dst, register const char *src) {
	while((*dst++ = *src++));
	return dst - 1;
}
inline char * strnmov(register char *dst, register const char *src, register int len) {
	while((*dst++ = *src++) && --len >= 0);
	return dst - 1;
}


inline void strtolower(char *c) {
	for(; *c; c++) *c = tolower(*c);
}
inline void strtolower2(char *c, int len) {
	for(; len >= 0; c++, len--) *c = tolower(*c);
}

inline char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}
inline char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

inline char *url_encode_mov(char *out, char *in) {
	while(*in) {
		if(isalnum(*in) || *in == '-' || *in == '_' || *in == '.' || *in == '~') *out++ = *in;
		else if(*in == ' ') *out++ = '+';
		else *out++ = '%', *out++ = to_hex(*in >> 4), *out++ = to_hex(*in & 15);
		in++;
	}
	*out = 0;
	return out;
}
inline char * url_decode(char *str) {
	char *p = str;
	char *end = str + strlen(str);
	for(p = str; *p; p++) {
		switch(*p) {
		case '%':
			if(p[1] && p[2]) {
				*p = from_hex(p[1]) << 4 | from_hex(p[2]);
				memcpy(p + 1, p + 3, end - p);
			}
			break;
		case '+':
			*p = ' ';
			break;
		}
	}
	return str;
}

#define htmlspecialchars_TAG(T, S, L) case T: memcpy(o, S, L); o += L; break;
inline char * htmlspecialchars(char *in, char *out) {
	char *o = out;
	while(*in) {
		switch(*in) {
		htmlspecialchars_TAG('&', "&amp;", 5);
		htmlspecialchars_TAG('<', "&lt;", 4);
		htmlspecialchars_TAG('>', "&gt;", 4);
		htmlspecialchars_TAG('"', "&quot;", 6);
		default: *o++ = *in; break;
		}
		in++;
	}
	*o = 0;
	return out;
}
inline char * htmlspecialchars_mov(char *out, char *in) {
	char *o = out;
	while(*in) {
		switch(*in) {
		htmlspecialchars_TAG('&', "&amp;", 5);
		htmlspecialchars_TAG('<', "&lt;", 4);
		htmlspecialchars_TAG('>', "&gt;", 4);
		htmlspecialchars_TAG('"', "&quot;", 6);
		default: *o++ = *in; break;
		}
		in++;
	}
	*o = 0;
	return o;
}
inline char * htmlspecialchars_nmov(char *out, char *in, int len) {
	char *o = out;
	while(*in && --len >= 0) {
		switch(*in) {
		htmlspecialchars_TAG('&', "&amp;", 5);
		htmlspecialchars_TAG('<', "&lt;", 4);
		htmlspecialchars_TAG('>', "&gt;", 4);
		htmlspecialchars_TAG('"', "&quot;", 6);
		default: *o++ = *in; break;
		}
		in++;
	}
	*o = 0;
	return o;
}
#define htmlspecialchars2_TAG(T, S, L) case T: memmove(p + L, p + 1, len - (p - in)); memcpy(p, S, L); p += L; len += L; break;
inline int htmlspecialchars2(char *in, int len) {
	char *p = in;
	while(*p) {
		switch(*p) {
		htmlspecialchars2_TAG('&', "&amp;", 5);
		htmlspecialchars2_TAG('<', "&lt;", 4);
		htmlspecialchars2_TAG('>', "&gt;", 4);
		htmlspecialchars2_TAG('"', "&quot;", 6);
		default: p++; break;
		}
	}
	return p - in;
}

inline int str_replace(const char *from, const int from_len, const char *to, const int to_len, char *in, int len) {
	char *p;
	int diff;
	p = in;
	diff = from_len - to_len;
	while((p = strstr(p, from)) != NULL) {
		if(diff) {
			memmove(p + to_len, p + from_len, len - (p - in));
			len -= diff;
		}
		if(to_len) memcpy(p, to, to_len);
		p += to_len;
	}
	return len;
}
inline char * str_replace_mov(char *out, char *in, const char *from, const int from_len, const char *to, const int to_len) {
	char *p = in;
	for(;;) {
		p = strstr(in, from);
		if(!p) break;
		if(p != in) {
			memcpy(out, in, p - in);
			out += p - in;
		}
		memcpy(out, to, to_len);
		out += to_len;
		in = p + from_len;
	}
	while(*in) *out++ = *in++;
	return out;
}

#endif
