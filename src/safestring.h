static inline char *strlcpy(char *restrict dst, const char *restrict src, size_t sz)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
	strncpy(dst, src, sz - 1);
#pragma GCC diagnostic pop
	dst[sz - 1] = '\0';
	return dst;
}

static inline char *strlcat(char *restrict dst, const char *restrict src, size_t sz)
{
	size_t dl = strlen(dst);
	if (dl < sz - 1)
		strlcpy(dst + dl, src, sz - dl);
	return dst;
}
