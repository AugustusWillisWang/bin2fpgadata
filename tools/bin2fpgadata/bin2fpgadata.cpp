// Convert .bin into tcl friendly .txt
// (and generate tcl scripts if necessary) 

#include <stdio.h>
#include <assert.h>
#include "cmdline.h" 

#define byte2nchar(b) (b << 1)

using namespace std;

inline char lowbyte2hex(unsigned char in){
  switch(in % 0x10) {
    case 0x0: return '0';
    case 0x1: return '1';
    case 0x2: return '2';
    case 0x3: return '3';
    case 0x4: return '4';
    case 0x5: return '5';
    case 0x6: return '6';
    case 0x7: return '7';
    case 0x8: return '8';
    case 0x9: return '9';
    case 0xa: return 'a';
    case 0xb: return 'b';
    case 0xc: return 'c';
    case 0xd: return 'd';
    case 0xe: return 'e';
    case 0xf: return 'f';
    default: assert(0);
  }
}

inline char highbyte2hex(unsigned char in){
  switch(in >> 4) {
    case 0x0: return '0';
    case 0x1: return '1';
    case 0x2: return '2';
    case 0x3: return '3';
    case 0x4: return '4';
    case 0x5: return '5';
    case 0x6: return '6';
    case 0x7: return '7';
    case 0x8: return '8';
    case 0x9: return '9';
    case 0xa: return 'a';
    case 0xb: return 'b';
    case 0xc: return 'c';
    case 0xd: return 'd';
    case 0xe: return 'e';
    case 0xf: return 'f';
    default: assert(0);
  }
}

inline void write_tcl(FILE* out_fp, int addr, char* data){
  fprintf(out_fp, "create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] -address %x -data %s -len 256 -burst INCR -size 32 -type write\n",
    addr,
    data
  );
}

int main(int argc, char *argv[])
{
  // create a parser
  cmdline::parser a;

  a.add<string>("in", 'i', "input binary file", true, "");
  a.add<string>("out", 'o', "output data text file", false, "data.txt");
  a.add<unsigned int>("burst", 'b', "burst length", false, 0x400);
  a.add("noeof", '\0', "disable end of file char 0a");
  a.add("tcl", '\0', "generate tcl script");
  a.add<unsigned int>("offset", '\0', "address offset", false, 0x0);
  a.add<unsigned int>("hole-begin", '\0', "address hole begin", false, 0x0);
  a.add<unsigned int>("hole-end", '\0', "address hole end", false, 0x0);

  // Run parser.
  // It returns only if command line arguments are valid.
  // If arguments are invalid, a parser output error msgs then exit program.
  // If help flag ('--help' or '-?') is specified, a parser output usage message then exit program.
  a.parse_check(argc, argv);

  cout << "Convert binary file to fpga data txt: " << endl
       << "input file: "
       << a.get<string>("in") << endl
       << "output file: "
       << a.get<string>("out") << endl
       << "data burst length: "
       << a.get<unsigned int>("burst") << endl;
  
  unsigned int offset = a.get<unsigned int>("offset");
  unsigned int hole_begin = a.get<unsigned int>("hole-begin");
  unsigned int hole_end = a.get<unsigned int>("hole-end");
  bool have_hole = false;

  if(offset > 0) {
    printf("add offset 0x%x to all addresses\n", offset);
  }

  if(hole_begin < hole_end) {
    printf("add address hole [0x%x - 0x%x)\n", hole_begin, hole_end);
    have_hole = true;
  }

  FILE* in_fp = fopen(a.get<string>("in").c_str(), "r");
  if(in_fp == NULL) {
    cout << "[ERROR] Invalid input file" << endl;
    return 1;
  }

  FILE* out_fp = fopen(a.get<string>("out").c_str(), "w");
  if(out_fp == NULL) {
    cout << "[ERROR] Invalid output file" << endl;
    return 1;
  }

  FILE* tcl_fp;
  if(a.exist("tcl")) {
    tcl_fp = fopen("data.tcl", "w");
    if(tcl_fp == NULL) {
      cout << "[ERROR] Invalid output file" << endl;
      return 1;
    }
  }

  const unsigned int burst_length = a.get<unsigned int>("burst");

  fseek(in_fp, 0, SEEK_END);
  unsigned int input_size = ftell(in_fp);
  fseek(in_fp, 0, SEEK_SET);
  unsigned int cur_position = 0;
  cout << "input size: "
       << input_size << endl;

  char burst_buffer[byte2nchar(0x400) + 2];
  assert(burst_length <= 0x400);
  assert((burst_length % 0x10) == 0);

  burst_buffer[byte2nchar(burst_length) + 1] = '\0';
  while((input_size - cur_position) >= burst_length) {
    for(int i = 0; i < burst_length; i++){
      char buf = 0;
      fread((char *)&buf, 1, 1, in_fp);
      burst_buffer[byte2nchar(burst_length - i - 1)] = highbyte2hex(buf);
      burst_buffer[byte2nchar(burst_length - i - 1) + 1] = lowbyte2hex(buf);
      // printf("%d: %02x -> 0x%x\n", byte2nchar(burst_length - i - 1), buf, &burst_buffer[byte2nchar(burst_length - i - 1)]);
      // printf("---%s---", burst_buffer);
    }
    burst_buffer[byte2nchar(burst_length)] = '\0';
    int cmd_addr = cur_position + offset;
    if(!(have_hole && (hole_begin <= cmd_addr) && (cmd_addr < hole_end))){
      fprintf(out_fp, "%x\n", cmd_addr);
      fprintf(out_fp, "%s\n", burst_buffer);
      if(a.exist("tcl")) {
        write_tcl(tcl_fp, cmd_addr, burst_buffer);
      }
    }
    cur_position += burst_length;
  }

  // fulfill the last burst
  int cmd_addr = cur_position + offset;
  fprintf(out_fp, "%x\n", cmd_addr);
  for(int i = 0; i < byte2nchar(burst_length); i++) {
    burst_buffer[i] = '0';
  }
  int i = 0;
  for(; i < (input_size - cur_position); i++){
    char buf = 0;
    fread((char *)&buf, 1, 1, in_fp);
    // printf("%x %x read: %c\n", i, cur_position+i, buf);
    burst_buffer[byte2nchar(burst_length - i - 1)] = highbyte2hex(buf);
    burst_buffer[byte2nchar(burst_length - i - 1) + 1] = lowbyte2hex(buf);
  }
  if(!a.exist("noeof")){
    printf("add eof char: '0a'\n");
    burst_buffer[byte2nchar(burst_length - i - 1) + 1] = 'a';
  }
  burst_buffer[byte2nchar(burst_length)] = '\0';
  fprintf(out_fp, "%s\n", burst_buffer);
  if(a.exist("tcl")) {
    write_tcl(tcl_fp, cmd_addr, burst_buffer);
    fclose(tcl_fp);
    printf("tcl script generated\n");
  }

  fclose(in_fp);
  fclose(out_fp);
  return 0;
}