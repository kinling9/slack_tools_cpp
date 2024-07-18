#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "dm/dm.h"
#include "parser/leda_rpt.h"
#include "utils/utils.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <gzipped input file>" << std::endl;
    return 0;
  }
  // Read from the first command line argument, assume it's gzipped
  auto start = std::chrono::high_resolution_clock::now();
  std::ifstream file(argv[1], std::ios_base::in | std::ios_base::binary);
  boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
  inbuf.push(boost::iostreams::gzip_decompressor());
  inbuf.push(file);
  // Convert streambuf to istream
  std::istream instream(&inbuf);

  leda_rpt_parser parser;
  parser.parse(instream);

  file.close();
  parser.print_paths();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n";
  return 0;
}
