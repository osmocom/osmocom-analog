
void nmt_country_list(int nmt_system);
int nmt_country_by_short_name(int nmt_system, const char *short_name);
const char *nmt_long_name_by_short_name(int nmt_system, const char *short_name);
int nmt_ta_by_short_name(int nmt_system, const char *short_name, int ta);
double nmt_channel2freq(int nmt_system, const char *short_name, int channel, int uplink, double *deviation_factor, int *scandinavia, int *tested);

