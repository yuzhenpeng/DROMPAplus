/* Copyright(c) Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _PW_GV_H_
#define _PW_GV_H_

#include <fstream>
#include <numeric>
#include "WigStats.hpp"
#include "GenomeCoverage.hpp"
#include "GCnormalization.hpp"
#include "SSP/src/MThread.hpp"
#include "SSP/src/LibraryComplexity.hpp"
#include "SSP/src/Mapfile.hpp"
#include "SSP/src/ssp_shiftprofile.hpp"

namespace RPM {
  class Pnorm {
    MyOpt::Opts opt;
    std::string ntype;
    int32_t nrpm;
    double ndepth;

  public:
    Pnorm():
      opt("Total Read normalization",100)
    {
      opt.add_options()
	("ntype,n",
	 boost::program_options::value<std::string>()->default_value("NONE"),
	 "Total read normalization\n{NONE|GR|GD|CR|CD}\n   NONE: not normalize\n   GR: for whole genome, read number\n   GD: for whole genome, read depth\n   CR: for each chromosome, read number\n   CD: for each chromosome, read depth")
	("nrpm",
	 boost::program_options::value<int32_t>()->default_value(2*NUM_10M)->notifier(boost::bind(&MyOpt::over<int32_t>, _1, 0, "--nrpm")),
	 "(For GR|CR) Total read number after normalization")
	("ndepth",
	 boost::program_options::value<double>()->default_value(1.0)->notifier(boost::bind(&MyOpt::over<double>, _1, 0, "--ndepth")),
	 "(For GD|CD) Averaged read depth after normalization")
	;
    }

    void setOpts(MyOpt::Opts &allopts) {
      allopts.add(opt);
    }
    
    void setValues(const MyOpt::Variables &values) {
      DEBUGprint("Pnorm setValues...");
      ntype  = values["ntype"].as<std::string>();
      nrpm   = values["nrpm"].as<int32_t>();
      ndepth = values["ndepth"].as<double>();
      if(ntype != "NONE" && ntype != "GR" && ntype != "GD" && ntype != "CR" && ntype != "CD") PRINTERR("invalid --ntype.\n");
      DEBUGprint("Pnorm setValues done.");
    }
    void dump() const {
      std::cout << "\nTotal read normalization: " << ntype << std::endl;
      if(ntype == "GR" || ntype == "CR"){
	std::cout << "\tnormed read: " << nrpm << " for genome" << std::endl;
      }
      else if(ntype == "GD" || ntype == "CD"){
	std::cout << "\tnormed depth: " << ndepth << std::endl;
      }
      printf("\n");
    }
    
    const std::string & getType() const { return ntype; }
    int32_t getnrpm()  const { return nrpm; }
    double getndepth() const { return ndepth; }
  };
}

class Mapfile: private Uncopyable {
  MyOpt::Opts opt;

  int32_t on_bed;
  std::string bedfilename;
  std::vector<bed> vbed;

  std::string samplename;
  std::string oprefix;
  std::string obinprefix;
  std::string mpdir;
  double mpthre;

  bool Greekchr;

  std::vector<Peak> vPeak;
  int32_t id_longestChr;

  // GC bias
  int32_t maxGC;

 public:
  SeqStatsGenome genome;
  WigStatsGenome wsGenome;
  RPM::Pnorm rpm;
  GenomeCov::Genome gcov;
  GCnorm gc;

  // for SSP
  SSPstats sspst;

  class LibComp complexity;
  
  Mapfile():
    opt("Fragment",100),
    mpdir(""), mpthre(0),
    Greekchr(false), 
    id_longestChr(0),
    maxGC(0), genome(),
    complexity()
  {
    opt.add_options()
      ("bed", boost::program_options::value<std::string>(),
       "specify the BED file of enriched regions (e.g., peak regions)")
      ("mp",  boost::program_options::value<std::string>(), "directory of mappability file")
      ("mpthre",
       boost::program_options::value<double>()->default_value(0.3)->notifier(boost::bind(&MyOpt::over<double>, _1, 0, "--mpthre")),
       "Threshold of low mappability regions")
      ;
  }
    
  void setOpts(MyOpt::Opts &allopts) {
    genome.setOpts(allopts);
    wsGenome.setOpts(allopts);
    rpm.setOpts(allopts);
    complexity.setOpts(allopts);
    sspst.setOpts(allopts);
    allopts.add(opt);
    gc.setOpts(allopts);
  }

  void setValues(const MyOpt::Variables &values);
  void dump() const {
    if(isBedOn()) std::cout << "Bed file: " << getbedfilename() << std::endl;
    if(mpdir != "") {
      printf("Mappability normalization:\n");
      std::cout << "\tFile directory: " << mpdir << std::endl;
      std::cout << "\tLow mappablitiy threshold: " << mpthre << std::endl;
    }
    if(gc.isGcNormOn()) {
      printf("Correcting GC bias:\n");
      std::cout << "\tChromosome directory: " << gc.getGCdir() << std::endl;
    }
  }

  int32_t getIdLongestChr () const { return id_longestChr; }
  int32_t isBedOn () const { return on_bed; }
  const std::string & getbedfilename() const { return bedfilename; }
  const std::string & getSampleName() const { return samplename; }
  const std::string & getMpDir()      const { return mpdir; }

  int32_t getmaxGC() const {return maxGC; }

  void calcGenomeCoverage() {
    std::cout << "calculate genome coverage.." << std::flush;

    gcov.setr4cmp(genome.getnread_nonred(Strand::BOTH), genome.getnread_inbed());

    for(size_t i=0; i<genome.chr.size(); i++) {
      auto array = GenomeCov::makeGcovArray(*this, genome.chr[i], gcov.getr4cmp());
      GenomeCov::Chr chr(array, gcov.getlackOfRead());
      gcov.chr.push_back(chr);
    }
    std::cout << "done." << std::endl;
  }

  void printPeak() const {
    std::string filename = getbinprefix() + ".peak.xls";
    std::ofstream out(filename);

    vPeak[0].printHead(out);
    for(uint32_t i=0; i<vPeak.size(); ++i) {
      vPeak[i].print(out, i, wsGenome.getbinsize());
    }
  }
  const std::string & getprefix() const { return oprefix; }
  const std::string & getbinprefix() const { return obinprefix; }
  double getmpthre() const { return mpthre; }
  const std::vector<bed> & getvbedref() const { return vbed; }

  void addPeak(const Peak &peak) {
    vPeak.push_back(peak);
  }
  void renewPeak(const int32_t i, const double val, const double p) {
    vPeak[vPeak.size()-1].renew(i, val, p);
  }
  void setFRiP() {
    if(isBedOn()) { 
    std::cout << "calculate FRiP score.." << std::flush;
    for(auto &x: genome.chr) x.setFRiP(vbed);
    std::cout << "done." << std::endl;
    }
  }

  void normalizeByGCcontents() {
    if(gc.isGcNormOn()) {
      std::cout << "chromosome for GC distribution: chr"
		<< genome.chr[id_longestChr].getname() << std::endl;
      GCdist d(genome.dflen.getflen(), gc);

      d.calcGCdist(genome.chr[id_longestChr], gc, getMpDir(), isBedOn(), vbed);
      maxGC = d.getmaxGC();

      std::string filename = getprefix() + ".GCdist.xls";
      d.outputGCweightDist(filename);

      weightRead(genome, d, gc.getGCdir());

      return;
    }
  }
};

#endif /* _PW_GV_H_ */