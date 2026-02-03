#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <map>
#include <string>

#define INFOHASH_SIZE 20
#define PEERID_SIZE 20

enum TF_Key {
  ANNOUNCE,
  ANNOUNCE_LIST,  // Common, but not guaranteed
  INFO,
  FILES,
  LENGTH,
  PATH,
  FLENGTH,  // Exists only for single-file torrents
  FNAME,    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  PIECE_LENGTH,
  PIECES,  // SHA-1 Hash List.
};

inline std::map<TF_Key, std::string> TF_String = {
    {ANNOUNCE, "announce"},
    {ANNOUNCE_LIST, "announce-list"},
    {INFO, "info"},
    {FILES, "files"},
    {LENGTH, "length"},
    {PATH, "path"},
    {FLENGTH, "length"},
    {FNAME, "name"},
    {PIECE_LENGTH, "piece-length"},
    {PIECES, "pieces"}};

#endif