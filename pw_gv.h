/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * This file is a part of DROMPA sources.
 */
#ifndef _PW_GV_H_
#define _PW_GV_H_

#include <fstream>
#include <boost/format.hpp>
#include "seq.h"
#include "common.h"
#include "util.h"
#include "warn.h"
#include "macro.h"
#include "readdata.h"

using namespace std;
using namespace boost::program_options;

/* default parameter */
#define FRAGMENT_LEN 150
#define MAX_FRAGMENT_LEN 500
#define NUM4RPM_DEFAULT 20000000
#define NUM4DEPTH_DEFAULT 1.0
#define NUM4CMP_DEFAULT 10
#define FLEN4GC_DEFAULT 120

#define DIST_READLEN_MAX 200
#define DIST_FRAGLEN_MAX 1000

#define NUM_DARRAY 100
#define READARRAY_NUM 50000 

#define HD_FROM 500
#define HD_WIDTH 1500

class WigStatsMember {
  int max;
  int *darray_all, *darray_bg;
  int num;
  double ave, var;
  double nb_p, nb_n, nb_p0;  /* for negative binomial model */
};

class WigStats{
  int n_darray;
  int thre;
  int num95;
  WigStatsMember *genome, *chr;
};

class Dist{
 public:
  int lenF3;
  int lenF5;
  int eflen;
  vector<int> readlen;
  vector<int> readlen_F5;
  vector<int> fraglen;
  // Hamming distance plot;
  vector<int> hd;

 Dist(): lenF3(0), lenF5(0), eflen(0) {
    vector<int> v(DIST_READLEN_MAX,0);
    readlen = v;
    vector<int> v2(DIST_READLEN_MAX,0);
    readlen_F5 = v2;
    vector<int> v3(DIST_FRAGLEN_MAX,0);
    fraglen = v3;
    vector<int> h(HD_WIDTH,0);
    hd = h;
  }
  void setlenF3() { lenF3 = getmaxi(readlen); }
  void setlenF5() { lenF5 = getmaxi(readlen_F5); }
  void setFraglen() { eflen = getmaxi(fraglen); }
};

class Fragment {
public:
  string name;
  string chr;
  int F3;
  Strand strand;
  int fraglen;
  int readlen_F3;
  
 Fragment(vector<string> v, bool pair):
  name(v[0]), chr(v[2]), fraglen(0), readlen_F3(v[9].length())
    {
      if(pair) fraglen = abs(stoi(v[8]));
      int sv = stoi(v[1]); // bitwise FLAG
      if(sv&16) {
	strand = STRAND_MINUS;
	F3 = stoi(v[3]) + readlen_F3 -1; //SAM
      } else {
	strand = STRAND_PLUS;
	F3 = stoi(v[3]) -1;  //SAM
      }
    }
  void print() {
    cout << name << ", " << chr << ", " << F3 << ", "<< strand << ", " << "fraglen " << fraglen << "," <<readlen_F3 << endl;
  }
};

class Read {
 public:
  int F3;
  int F5;
  int weight;
  int duplicate;
  int inpeak;
 Read(const Fragment &frag): F3(frag.F3), weight(1), duplicate(0), inpeak(0) {
    if(frag.strand == STRAND_PLUS) F5 = frag.F3 + frag.fraglen;
    else F5 = frag.F3 - frag.fraglen;
  }
};

class strandData {
 public:
  vector<Read> vRead;
  long nread;
  long nread_nonred;
  long nread_red;
  double nread_rpm;
  double nread_afterGC;

 strandData(): nread(0), nread_nonred(0), nread_red(0), nread_rpm(0), nread_afterGC(0) {}
  void setnread() { nread = vRead.size(); }
  void print() {
    cout << nread << "\t" << nread_nonred << "\t" << nread_red << "\t" << nread_rpm << "\t" << nread_afterGC << endl;
  }
  void printnonred(ofstream &out)  { printr(out, nread_nonred,  nread); }
  void printred(ofstream &out)     { printr(out, nread_red,     nread); }
  void printafterGC(ofstream &out) { printr(out, nread_afterGC, nread); }
};

class SeqStats {
 public:
  string name;
  long len, len_mpbl;
  int nbin;
  double p_mpbl;  /* mappability */
  // genome coverage
  long nbp, ncov, ncovnorm;
  double gcovRaw, gcovNorm;

  strandData seq[STRANDNUM];
  double depth;
  double w;
  /* FRiP */
  long nread_inbed;
  double FRiP;

 SeqStats(string s, int l=0): name(s),len(l), len_mpbl(l), nbin(0), p_mpbl(0), nbp(0), ncov(0), ncovnorm(0), gcovRaw(0), gcovNorm(0), depth(0), w(0), nread_inbed(0), FRiP(0) {}
  void addfrag(const Fragment &frag) {
    Read r(frag);
    seq[frag.strand].vRead.push_back(r);
  }
  long bothnread ()         { return seq[STRAND_PLUS].nread         + seq[STRAND_MINUS].nread; }
  long bothnread_nonred ()  { return seq[STRAND_PLUS].nread_nonred  + seq[STRAND_MINUS].nread_nonred; }
  long bothnread_red ()     { return seq[STRAND_PLUS].nread_red     + seq[STRAND_MINUS].nread_red; }
  long bothnread_rpm ()     { return seq[STRAND_PLUS].nread_rpm     + seq[STRAND_MINUS].nread_rpm; }
  long bothnread_afterGC () { return seq[STRAND_PLUS].nread_afterGC + seq[STRAND_MINUS].nread_afterGC; }

  void addnread(const SeqStats &x) { 
    for(int i=0; i<STRANDNUM; i++) {
      seq[i].nread += x.seq[i].nread;
      //      cout <<seq[i].nread <<"\t" << x.seq[i].nread<< endl;
    }
  }
  void addnread_red(const SeqStats &x) { 
    for(int i=0; i<STRANDNUM; i++) {
      seq[i].nread_nonred += x.seq[i].nread_nonred;
      seq[i].nread_red    += x.seq[i].nread_red;
    }
  }
  void addGcov(const SeqStats &x) {
    nbp      += x.nbp;
    ncov     += x.ncov;
    ncovnorm += x.ncovnorm;
    gcovRaw  = nbp ? ncov / (double)nbp: 0;
    gcovNorm = nbp ? ncovnorm / (double)nbp: 0;
  }
  void print() {
    cout << name << "\t" << len << "\t" << len_mpbl << "\t" << bothnread() << "\t" << bothnread_nonred() << "\t" << bothnread_red() << "\t" << bothnread_rpm() << "\t" << bothnread_afterGC()<< "\t" << depth << endl;
  }
  void calcdepth(int flen) {
    depth = len_mpbl ? bothnread_nonred() * flen / (double)len_mpbl: 0;
  }
  void calcGcov(const vector<char> &array) {
    for(int i=0; i<len; ++i) {
      if(array[i] == MAPPABLE || array[i] == COVREAD_ALL || array[i] == COVREAD_NORM) ++nbp;
      if(array[i] == COVREAD_ALL || array[i] == COVREAD_NORM) ++ncov;
      if(array[i] == COVREAD_NORM) ++ncovnorm;
    }
    gcovRaw  = nbp ? ncov / (double)nbp: 0;
    gcovNorm = nbp ? ncovnorm / (double)nbp: 0;
  }
  void setF5(int flen) {
    int d;
    for(int strand=0; strand<STRANDNUM; ++strand) {
      if(strand == STRAND_PLUS) d = flen; else d = -flen;
      for (auto &x: seq[strand].vRead) x.F5 = x.F3 + d;
    }
  }
  void setWeight(double weight) {
    w = weight;
    for(int i=0; i<STRANDNUM; i++) seq[i].nread_rpm = seq[i].nread_nonred * w;
  }
  void calcFRiP(const vector<bed> vbed) {
    vector<char> array(len,MAPPABLE);
    arraySetBed(array, name, vbed);
    for(int strand=0; strand<STRANDNUM; ++strand) {
      for (auto &x: seq[strand].vRead) {
	if(x.duplicate) continue;
	int s(min(x.F3, x.F5));
	int e(max(x.F3, x.F5));
	for(int i=s; i<=e; ++i) {
	  if(array[i]==INBED) {
	    x.inpeak = 1;
	    nread_inbed++;
	    break;
	  }
	}
      }
    }
    FRiP = nread_inbed/(double)bothnread_nonred();
  }
};
  
class Mapfile {
public:
  string oprefix;
  Dist dist;
  SeqStats genome;
  vector<SeqStats> chr;
  vector<SeqStats>::iterator lchr; // longest chromosome

  string lastchr;
  int flen_def;

  // PCR bias
  int thre4filtering;
  int nt_all, nt_nonred, nt_red;
  int tv, gv;
  double r4cmp;
  vector<bed> vbed;

  // GC bias
  vector<double> GCweight;
  int maxGC;

  Mapfile(const variables_map &values);
  void addF5(const int readlen_F5) { dist.readlen_F5[readlen_F5]++; }
  void addfrag(const Fragment &frag) {
    dist.readlen[frag.readlen_F3]++;
    dist.fraglen[frag.fraglen]++;
    int on(0);
    for(auto &x:chr) {
      if(x.name == frag.chr) {
	x.addfrag(frag);
	on++;
      }
    }
    if(!on) cerr << "Warning: " << frag.chr << " is not in genometable." << endl;
  }
  void calcdepth(const variables_map &values) {
    int flen;
    if(!values.count("nomodel")) flen = dist.eflen;
    else flen = flen_def;
    for (auto &x:chr) x.calcdepth(flen);
    genome.calcdepth(flen);
  }
  void setF5(const variables_map &values) {
    int flen;
    if(!values.count("nomodel")) flen = dist.eflen;
    else flen = flen_def;
    for (auto &x:chr) x.setF5(flen);
  }
  void setnread() {
    for (auto &x:chr) {
      for(int i=0; i<STRANDNUM; i++) x.seq[i].setnread();
      genome.addnread(x);
    }
  }
  void setnread_red() {
    for (auto &x:chr) genome.addnread_red(x);
  }
  double complexity() { return nt_nonred/(double)nt_all; }
  void printstats() {
    for (auto x:chr) x.print();
    genome.print();
  }
  void calcFRiP() {
    cout << "calculate FRiP score.." << flush;
    for(auto &c: chr) {
      c.calcFRiP(vbed);
      genome.nread_inbed += c.nread_inbed;
    }
    genome.FRiP = genome.nread_inbed/(double)genome.bothnread_nonred();
    
    cout << "done." << endl;
    return;
  }
};

#endif /* _PW_GV_H_ */
