
void init_station(void);
void station_list(void);
const char *get_station_name(uint8_t nat, uint8_t fuvst, uint8_t rest, const char **standort);
const char *get_station_id(const char *name, uint8_t *nat, uint8_t *fuvst, uint8_t *rest);

