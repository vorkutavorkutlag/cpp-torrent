#include "client.h"

#include <unistd.h>

#include <ctime>

#include "constants.h"

/* Sets new seed for random and
  randomly generates PEERID_SIZE length string
  of alphanumeric characters.*/
std::string generate_peerid() {
  static const char alnum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  std::string tmp_s;
  tmp_s.reserve(PEERID_SIZE);

  srand(time(NULL) * getpid());

  for (int i = 0; i < PEERID_SIZE; ++i) {
    tmp_s += alnum[rand() % (sizeof(alnum) - 1)];
  }

  return tmp_s;
}