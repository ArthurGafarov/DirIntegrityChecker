#pragma once

void init_crc_table(void);
void calc_crc(const char *in_file, unsigned int *crc);
