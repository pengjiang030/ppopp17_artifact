#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <sys/time.h>
#include <limits.h>
#include "immintrin.h"
using namespace std;

#define BLOCK 4096

int num_states;
int num_alphs;
int *trans_table;
int *spec_table_temp;
__m512i *vind;
map<char, int> lookup_table;
int cur_in = 0;
__m512i *spec_mm_table;
__mmask16 least_one[65536];
int least_one_pos[65536];


void input_rex(char *filename)
{
 ifstream fin(filename); 
 string line;
 getline(fin, line);
 stringstream sin(line);
 sin >> num_states >> num_alphs;
 trans_table = (int *)_mm_malloc(sizeof(int)*num_states*(num_alphs+1), 64);
 for(int i=0;i<=num_alphs;i++) {
  for(int j=0;j<num_states;j++) {
    trans_table[i*num_states+j] = 0;
  }
 }

 int ss;
 getline(fin, line);
 stringstream sin1(line);
// while(sin1 >> ss) {
//   trans_table[num_alphs*num_states+ss] = 1;
// }

 while(getline(fin, line)) {
  stringstream sin2(line);
  int s, e;
  char in;
  sin2 >> s >> in >> e;

  if(in=='^') {
    for(int i=0;i<=num_alphs;i++) trans_table[i*num_states+s] = e;
  } else {
    map<char, int>::iterator it = lookup_table.find(in);
    if(it != lookup_table.end())
      trans_table[(it->second)*num_states+s] = e;
    else {
      lookup_table[in] = cur_in;
      trans_table[cur_in*num_states+s] = e;
      cur_in++;
    }
  }
 }

 spec_mm_table = (__m512i *)_mm_malloc(sizeof(__m512i)*num_states, 64);

 int *spec_temp = (int *)_mm_malloc(sizeof(int)*16, 64);
 spec_table_temp = (int *)_mm_malloc(sizeof(int)*16*(num_alphs+1)*(num_alphs+1), 64);

 int *all_states = (int *)_mm_malloc(sizeof(int)*num_states, 64);
 int *to_states = (int *)_mm_malloc(sizeof(int)*num_states, 64);
 int *count = new int[num_states];
 int max, p;


 for(int i1=0;i1<=num_alphs;i1++) {
   for(int i2=0;i2<=num_alphs;i2++) {
     for(int k=0;k<num_states;k++) {
       to_states[k] = trans_table[trans_table[i1*num_states+k]+i2*num_states]; 
     }
     for(int k=0;k<num_states;k++) {
       count[k] = 0; 
     }

     for(int k=0;k<num_states;k++) {
       count[to_states[k]]++; 
     }

     for(int j=1;j<16;j++) {
       max = INT_MIN;
       for(int k=0;k<num_states;k++) {
         if(count[k] > max) {
           max = count[k];
           p = k;
         }
       }
       count[p] = -1;
       spec_table_temp[16*(i1*(num_alphs+1)+i2)+j] = p;
     }
   }
 }
//init the pos of the first 1 from least significant bit
  for(int i=2;i<65536;i++) {
    int c = 0;
    int t = i & 0xFFFE;
    while(t % 2 == 0) {
      t /= 2;
      c++;
    }
    least_one[i] = (1 << c);
    least_one_pos[i] = c;
  }

 for(int i=0;i<num_states;i++) {
   spec_temp[0] = i;
   int j = 1;
   for(;j<16 && j-1<num_states;j++) {
     spec_temp[j] = j-1;
   }
   for(;j<16;j++) {
     spec_temp[j] = 0;
   }
   spec_mm_table[i] = _mm512_load_epi32(spec_temp);
 }


 int *tmp_states = (int *)_mm_malloc(sizeof(int)*16, 64);
 vind = (__m512i *)_mm_malloc(sizeof(__m512i)*BLOCK, 64);
 for(int i=0;i<BLOCK;i++) {
   tmp_states[0] = i;
   for(int j=1;j<16;j++)tmp_states[j] = i+BLOCK;
   vind[i] = _mm512_load_epi32(tmp_states);
 }
}

void print_rex() 
{
  for(int i=0;i<=num_alphs;i++) {
    for(int j=0;j<num_states;j++) {
      cout << trans_table[i*num_states+j]<< " ";
    }
    cout << endl;
  }
}

void match(char *filename) 
{
  ifstream fin(filename);
  char c;
  int s = 0;
  int count = 0;
  string text;
  string line;
  while(getline(fin, line)) {
    text += line;
  }

  for(int i=0;i<text.length();i++) {
    if(text[i]==' ')text[i] = '|';
  }

  int *text_int = (int *)malloc(sizeof(int) * text.length());
  for(int i=0;i<text.length();i++) {
    map<char, int>::iterator it = lookup_table.find(text[i]);
    if(it!=lookup_table.end()) {
      text_int[i] = it->second;
    } else {
      text_int[i] = num_alphs;
    }
  }

  __m512i vsize = _mm512_set1_epi32(num_states);
  int *tmp_states = (int *) _mm_malloc(sizeof(int)*16, 64);
  __m512i vperm = _mm512_set_epi32(7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8);
  __m512i vperm1 = _mm512_set_epi32(14,13,12,11,6,5,4,3,2,1,4,3,2,1,0,15);
  __m512i vperm2 = _mm512_set_epi32(5,4,3,2,1,5,4,3,2,1,4,3,2,1,0,15);
  for(int i=0;i<16;i++) tmp_states[i] = 0;
  for(int i=0;i<16&&i<num_states;i++) tmp_states[i] = i;
  __m512i b = _mm512_load_epi32(tmp_states);
  __m512i vsindx = _mm512_set_epi32(3*BLOCK, 3*BLOCK, 3*BLOCK, 3*BLOCK, 3*BLOCK, 2*BLOCK, 2*BLOCK, 2*BLOCK, 2*BLOCK, 2*BLOCK, BLOCK, BLOCK, BLOCK, BLOCK, BLOCK, 0);

  float ppp=0,qqq=0;
  int cur = 0;
  __m512i vs;
  __m512i vss;
  int *vss_buffer = (int *)_mm_malloc(sizeof(int)*16, 64);
  int *vs_buffer = (int *)_mm_malloc(sizeof(int)*16, 64);
  struct timeval tv1, tv2;
  struct timezone tz1, tz2;
  gettimeofday(&tv1, &tz1);

  while(cur + 4*BLOCK < text.length()) {
    vs = _mm512_set1_epi32(s);

    int index1 = text_int[cur+BLOCK-2]*(num_alphs+1)+text_int[cur+BLOCK-1];
    __m512i ss1 = _mm512_load_epi32(spec_table_temp+index1*16);
    vs = _mm512_mask_mov_epi32(vs, 0x003E, ss1);

    int index2 = text_int[cur+2*BLOCK-2]*(num_alphs+1)+text_int[cur+2*BLOCK-1];
    __m512i ss2 = _mm512_load_epi32(spec_table_temp+index2*16);
    vs = _mm512_mask_permutevar_epi32(vs, 0x07C0, vperm1, ss2);

    int index3 = text_int[cur+3*BLOCK-2]*(num_alphs+1)+text_int[cur+3*BLOCK-1];
    __m512i ss3 = _mm512_load_epi32(spec_table_temp+index3*16);
    vs = _mm512_mask_permutevar_epi32(vs, 0xF800, vperm2, ss3);

    vss = vs;

    for(int i=cur;i<cur+BLOCK;i+=8) {
      __m512i vindx = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i));
      __m512i vp = _mm512_i32gather_epi32(vindx, text_int, 4);
      vp = _mm512_mullo_epi32(vp, vsize);
      vp = _mm512_add_epi32(vss, vp);
      vss = _mm512_i32gather_epi32(vp, trans_table, 4);

      __m512i vindx2 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+1));
      __m512i vp2 = _mm512_i32gather_epi32(vindx2, text_int, 4);
      vp2 = _mm512_mullo_epi32(vp2, vsize);
      vp2 = _mm512_add_epi32(vss, vp2);
      vss = _mm512_i32gather_epi32(vp2, trans_table, 4);


      __m512i vindx3 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+2));
      __m512i vp3 = _mm512_i32gather_epi32(vindx3, text_int, 4);
      vp3 = _mm512_mullo_epi32(vp3, vsize);
      vp3 = _mm512_add_epi32(vss, vp3);
      vss = _mm512_i32gather_epi32(vp3, trans_table, 4);


      __m512i vindx4 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+3));
      __m512i vp4 = _mm512_i32gather_epi32(vindx4, text_int, 4);
      vp4 = _mm512_mullo_epi32(vp4, vsize);
      vp4 = _mm512_add_epi32(vss, vp4);
      vss = _mm512_i32gather_epi32(vp4, trans_table, 4);

      __m512i vindx5 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+4));
      __m512i vp5 = _mm512_i32gather_epi32(vindx5, text_int, 4);
      vp5 = _mm512_mullo_epi32(vp5, vsize);
      vp5 = _mm512_add_epi32(vss, vp5);
      vss = _mm512_i32gather_epi32(vp5, trans_table, 4);

      __m512i vindx6 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+5));
      __m512i vp6 = _mm512_i32gather_epi32(vindx6, text_int, 4);
      vp6 = _mm512_mullo_epi32(vp6, vsize);
      vp6 = _mm512_add_epi32(vss, vp6);
      vss = _mm512_i32gather_epi32(vp6, trans_table, 4);

      __m512i vindx7 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+6));
      __m512i vp7 = _mm512_i32gather_epi32(vindx7, text_int, 4);
      vp7 = _mm512_mullo_epi32(vp7, vsize);
      vp7 = _mm512_add_epi32(vss, vp7);
      vss = _mm512_i32gather_epi32(vp7, trans_table, 4);

      __m512i vindx8 = _mm512_add_epi32(vsindx, _mm512_set1_epi32(i+7));
      __m512i vp8 = _mm512_i32gather_epi32(vindx8, text_int, 4);
      vp8 = _mm512_mullo_epi32(vp8, vsize);
      vp8 = _mm512_add_epi32(vss, vp8);
      vss = _mm512_i32gather_epi32(vp8, trans_table, 4);
    }
    ppp+=4;
    qqq++;
    cur += BLOCK;
    _mm512_store_epi32(vss_buffer, vss);
    _mm512_store_epi32(vs_buffer, vs);

    s = vss_buffer[0];
    for(int k=1;k<2;k++) {
      if(s==vs_buffer[k]) {
        qqq++;
        cur+=BLOCK;
        s = vss_buffer[k];

        for(int k1=6;k1<7;k1++) {
          if(s==vs_buffer[k1]) {
            qqq++;
            cur+=BLOCK;
            s = vss_buffer[k1];

            for(int k2=11;k2<12;k2++) {
              if(s==vs_buffer[k2]) {
                qqq++;
                cur+=BLOCK;
                s = vss_buffer[k2];
                break;
              }
            }

            break;
          }

        
        }
        break;
      }
    }
  }

  for(int i=cur;i<text.length();i++) {
      s = trans_table[text_int[i]*num_states+s];
  }

  gettimeofday(&tv2, &tz2);
  cout << "time used: " << tv2.tv_usec - tv1.tv_usec + 1000000*(tv2.tv_sec - tv1.tv_sec) << "ms"<<endl;
  cout << "final state: " << s << endl;
  cout << "success rate: " << qqq / ppp << endl;
}


int main(int argc, char *argv[])
{
  input_rex(argv[1]);
  match(argv[2]);
  return 0;
}
