/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * This file is a part of DROMPA sources.
 */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "SSP/src/pw_readmapfile.h"
#include "pw_makefile.h"
#include "pw_gc.h"
#include "version.h"
#include "SSP/src/pw_gv.h"
#include "SSP/src/ssp_shiftprofile.h"

namespace {
  const int numGcov(5000000);
}

MyOpt::Variables getOpts(int argc, char* argv[]);
void setOpts(MyOpt::Opts &);
void init_dump(const MyOpt::Variables &);
void output_stats(const MyOpt::Variables &values, const Mapfile &p);
void calcGenomeCoverage(const MyOpt::Variables &values, Mapfile &p);
void output_wigstats(Mapfile &p);

void printVersion()
{
  std::cerr << "parse2wig version " << VERSION << std::endl;
  exit(0);
}

void help_global()
{
  auto helpmsg = R"(
===============

Usage: parse2wig+ [option] -i <inputfile> -o <output> -gt <genome_table>)";
  
  std::cerr << "\nparse2wig v" << VERSION << helpmsg << std::endl;
  return;
}

void calcFRiP(SeqStats &chr, const std::vector<bed> &vbed)
{
  uint64_t nread_inbed(0);
  std::vector<int8_t> array(chr.getlen(), MAPPABLE);
  arraySetBed(array, chr.getname(), vbed);
  for(int strand=0; strand<STRANDNUM; ++strand) {
    for (auto &x: chr.getvReadref_notconst((Strand)strand)) {
      if(x.duplicate) continue;
      int s(std::min(x.F3, x.F5));
      int e(std::max(x.F3, x.F5));
      for(int i=s; i<=e; ++i) {
	if(array[i]==INBED) {
	  x.inpeak = 1;
	  ++nread_inbed;
	  break;
	}
      }
    }
  }
  chr.setFRiP(nread_inbed);
  return;
}

void setFRiP(SeqStatsGenome &genome)
{
  std::cout << "calculate FRiP score.." << std::flush;
  for(auto &x: genome.chr) calcFRiP(x, genome.getvbedref());
  std::cout << "done." << std::endl;
  return;
}

int main(int argc, char* argv[])
{
  MyOpt::Variables values = getOpts(argc, argv);
  
  boost::filesystem::path dir(values["odir"].as<std::string>());
  boost::filesystem::create_directory(dir);

  Mapfile p(values);
  read_mapfile(values, p);
  /* output distributions of read length and fragment length */
  p.outputDistFile(values);

  if(!values.count("nofilter")) {
    checkRedundantReads(values, p);
  }/* else {
    p.genome.setnread2nread_red();
    }*/
  //  p.genome.setnread_red();

  strShiftProfile(values, p, "jaccard");
  for (auto &x:p.genome.chr) calcdepth(x, p.getflen(values));
  calcdepth(p.genome, p.getflen(values));
  

  // BED file
  if (values.count("bed")) {
    p.genome.setbed(values["bed"].as<std::string>());
    setFRiP(p.genome);
  }

#ifdef DEBUG
  p.genome.printReadstats();
#endif

  // Genome coverage
  calcGenomeCoverage(values, p);
  
  // GC contents
  if (values.count("genome")) normalizeByGCcontents(values, p);
  
  // make and output wigdata
  makewig(values, p);
  
  p.estimateZINB();  // for genome
  
  p.printPeak(values);
  
  // output stats
  output_wigstats(p);
  output_stats(values, p);

  return 0;
}

void checkParam(const MyOpt::Variables &values)
{
  std::vector<std::string> intopts = {"binsize", "flen", "maxins" , "nrpm", "flen4gc", "threads"};
  for (auto x: intopts) chkminus<int>(values, x, 0);
  std::vector<std::string> intopts2 = {"rcenter", "thre_pb"};
  for (auto x: intopts2) chkminus<int>(values, x, -1);
  std::vector<std::string> dbopts = {"ndepth", "mpthre"};
  for (auto x: dbopts) chkminus<double>(values, x, 0);
  
  if(!my_range(values["of"].as<int>(), 0, PWFILETYPENUM-1)) PRINTERR("invalid wigfile type.\n");

  if(values.count("ftype")) {
    std::string ftype = values["ftype"].as<std::string>();
    if(ftype != "SAM" && ftype != "BAM" && ftype != "BOWTIE" && ftype != "TAGALIGN") PRINTERR("invalid --ftype.\n");
  }
  std::string ntype = values["ntype"].as<std::string>();
  if(ntype != "NONE" && ntype != "GR" && ntype != "GD" && ntype != "CR" && ntype != "CD") PRINTERR("invalid --ntype.\n");

  if (values.count("genome")) {
    //    if(!values.count("mp")) PRINTERR("--genome option requires --mp option.\n");
    isFile(values["genome"].as<std::string>());
  }
  
  return;
}

MyOpt::Variables getOpts(int argc, char* argv[])
{
  MyOpt::Opts allopts("Options");
  setOpts(allopts);
  
  MyOpt::Variables values;
  
  try {
    boost::program_options::parsed_options parsed = parse_command_line(argc, argv, allopts);
    store(parsed, values);
    
    if (values.count("version")) printVersion();

    if (argc ==1) {
      help_global();
      std::cerr << "Use --help option for more information on the other options\n\n";
      exit(0);
    }
    if (values.count("help")) {
      help_global();
      std::cout << "\n" << allopts << std::endl;
      exit(0);
    }
    std::vector<std::string> opts = {"input", "output", "gt"};
    for (auto x: opts) {
      if (!values.count(x)) PRINTERR("specify --" << x << " option.");
    }

    notify(values);

    checkParam(values);
    init_dump(values);
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    exit(0);
  }
  return values;
}

void setOpts(MyOpt::Opts &allopts)
{
  using namespace boost::program_options;

  MyOpt::Opts optreq("Required",100);
  optreq.add_options()
    ("input,i",   value<std::string>(), "Mapping file. Multiple files are allowed (separated by ',')")
    ("output,o",  value<std::string>(), "Prefix of output files")
    ("gt",        value<std::string>(), "Genome table (tab-delimited file describing the name and length of each chromosome)")
    ;
  MyOpt::Opts optIO("Input/Output",100);
  optIO.add_options()
    ("binsize,b",   value<int>()->default_value(50),	  "bin size")
    ("ftype,f",     value<std::string>(), "{SAM|BAM|BOWTIE|TAGALIGN}: format of input file\nTAGALIGN could be gzip'ed (extension: tagAlign.gz)")
    ("of",        value<int>()->default_value(0),	  "output format\n   0: binary (.bin)\n   1: compressed wig (.wig.gz)\n   2: uncompressed wig (.wig)\n   3: bedGraph (.bedGraph)\n   4: bigWig (.bw)")
    ("odir",        value<std::string>()->default_value("parse2wigdir+"),	  "output directory name")
    ("rcenter", value<int>()->default_value(0), "consider length around the center of fragment ")
    ;
  MyOpt::Opts optsingle("For single-end read",100);
  optsingle.add_options()
    ("nomodel",   "predefine the fragment length (default: estimated by hamming distance plot)")
    ("flen",        value<int>()->default_value(150), "predefined fragment length\n(Automatically calculated in paired-end mode)")
    ("nfcs",        value<int>()->default_value(10000000),   "read number for calculating fragment variability")
    ;
  MyOpt::Opts optpair("For paired-end read",100);
  optpair.add_options()
    ("pair", 	  "add when the input file is paired-end")
    ("maxins",        value<int>()->default_value(500), "maximum fragment length")
    ;
  MyOpt::Opts optpcr("PCR bias filtering",100);
  optpcr.add_options()
    ("nofilter", 	  "do not filter PCR bias")
    ("thre_pb",        value<int>()->default_value(0),	  "PCRbias threshold (default: more than max(1 read, 10 times greater than genome average)) ")
    ("ncmp",        value<int>()->default_value(10000000),	  "read number for calculating library complexity")
    ;
  MyOpt::Opts optnorm("Total read normalization",100);
  optnorm.add_options()
    ("ntype,n",        value<std::string>()->default_value("NONE"),  "Total read normalization\n{NONE|GR|GD|CR|CD}\n   NONE: not normalize\n   GR: for whole genome, read number\n   GD: for whole genome, read depth\n   CR: for each chromosome, read number\n   CD: for each chromosome, read depth")
    ("nrpm",        value<int>()->default_value(20000000),	  "Total read number after normalization")
    ("ndepth",      value<double>()->default_value(1.0),	  "Averaged read depth after normalization")
    ("bed",        value<std::string>(),	  "specify the BED file of enriched regions (e.g., peak regions)")
    ;  
  MyOpt::Opts optmp("Mappability normalization",100);
  optmp.add_options()
    ("mp",        value<std::string>(),	  "Mappability file")
    ("mpthre",    value<double>()->default_value(0.3),	  "Threshold of low mappability regions")
    ;
  MyOpt::Opts optgc("GC bias normalization\n   (require large time and memory)",100);
  optgc.add_options()
    ("genome",     value<std::string>(),	  "reference genome sequence for GC content estimation")
    ("flen4gc",    value<int>()->default_value(120),  "fragment length for calculation of GC distribution")
    ("gcdepthoff", "do not consider depth of GC contents")
    ;
  MyOpt::Opts optother("Others",100);
  optother.add_options()
    ("threads,p",    value<int>()->default_value(1),  "number of threads to launch")
    ("version,v", "print version")
    ("help,h", "show help message")
    ;
  MyOpt::Opts optssp("Strand shift profile",100);
  optssp.add_options()
    ("ng_from", value<int32_t>()->default_value(5*NUM_100K), "start shift of background")
    ("ng_to",   value<int32_t>()->default_value(NUM_1M),     "end shift of background")
    ("ng_step", value<int32_t>()->default_value(5000),       "step shift on of background")
    ("ng_from_fcs", value<int32_t>()->default_value(NUM_100K), "fcs start of background")
    ("ng_to_fcs",   value<int32_t>()->default_value(NUM_1M),   "fcs end of background")
    ("ng_step_fcs", value<int32_t>()->default_value(NUM_100K), "fcs step on of background")
    ("num4ssp", value<int32_t>()->default_value(NUM_10M),    "Read number for calculating backgroud uniformity (per 100 Mbp)")
    ("ssp_cc",    "make ssp based on cross correlation")
    ("ssp_hd",    "make ssp based on hamming distance")
    ("ssp_exjac", "make ssp based on extended Jaccard index")
    ("eachchr", "make chromosome-sparated ssp files")
    ("mptable", value<std::string>(), "Genome table for mappable regions")
    ;
  allopts.add(optreq).add(optIO).add(optsingle).add(optpair).add(optpcr).add(optnorm).add(optmp).add(optgc).add(optssp).add(optother);
  return;
}

void init_dump(const MyOpt::Variables &values){
  std::vector<std::string> str_wigfiletype = {"BINARY", "COMPRESSED WIG", "WIG", "BEDGRAPH", "BIGWIG"};
 
  BPRINT("\n======================================\n");
  BPRINT("parse2wig version %1%\n\n") % VERSION;
  BPRINT("Input file %1%\n")         % values["input"].as<std::string>();
  if(values.count("ftype")) BPRINT("\tFormat: %1%\n") % values["ftype"].as<std::string>();
  BPRINT("Output file: %1%/%2%\n")   % values["odir"].as<std::string>() % values["output"].as<std::string>();
  BPRINT("\tFormat: %1%\n")          % str_wigfiletype[values["of"].as<int>()];
  BPRINT("Genome-table file: %1%\n") % values["gt"].as<std::string>();
  BPRINT("Binsize: %1% bp\n")        % values["binsize"].as<int>();
  BPRINT("Number of threads: %1%\n") % values["threads"].as<int>();
  if (!values.count("pair")) {
    std::cout << "Single-end mode: ";
    BPRINT("fragment length will be estimated from hamming distance\n");
    if (values.count("nomodel")) BPRINT("Predefined fragment length: %1%\n") % values["flen"].as<int>();
    if(values["nfcs"].as<int>()) BPRINT("\t%1% reads used for fragment variability\n") % values["nfcs"].as<int>();
  } else {
    std::cout << "Paired-end mode: ";
    BPRINT("Maximum fragment length: %1%\n") % values["maxins"].as<int>();
  }
  if (!values.count("nofilter")) {
    BPRINT("PCR bias filtering: ON\n");
    if (values["thre_pb"].as<int>()) BPRINT("PCR bias threshold: > %1%\n") % values["thre_pb"].as<int>();
  } else {
    BPRINT("PCR bias filtering: OFF\n");
  }
  BPRINT("\t%1% reads used for library complexity\n") % values["ncmp"].as<int>();
  if (values.count("bed")) BPRINT("Bed file: %1%\n") % values["bed"].as<std::string>();

  std::string ntype = values["ntype"].as<std::string>();
  BPRINT("\nTotal read normalization: %1%\n") % ntype;
  if(ntype == "GR" || ntype == "CR"){
    BPRINT("\tnormed read: %1% M for genome\n") % (values["nrpm"].as<int>() /static_cast<double>(NUM_1M));
  }
  else if(ntype == "GD" || ntype == "CD"){
    BPRINT("\tnormed depth: %1%\n") % values["ndepth"].as<double>();
  }
  printf("\n");
  if (values.count("mp")) {
    printf("Mappability normalization:\n");
    BPRINT("\tFile directory: %1%\n") % values["mp"].as<std::string>();
    BPRINT("\tLow mappablitiy threshold: %1%\n") % values["mpthre"].as<double>();
  }
  if (values.count("genome")) {
    printf("Correcting GC bias:\n");
    BPRINT("\tChromosome directory: %1%\n") % values["genome"].as<std::string>();
  }
  printf("======================================\n");
  return;
}

template <class T>
void print_SeqStats(const MyOpt::Variables &values, std::ofstream &out, const T &p, const Mapfile &mapfile)
{
  /* genome data */
  out << p.getname() << "\t" << p.getlen()  << "\t" << p.getlenmpbl() << "\t" << p.getpmpbl() << "\t";
  /* total reads*/
  out << boost::format("%1%\t%2%\t%3%\t%4$.1f%%\t")
    % p.getnread(STRAND_BOTH) % p.getnread(STRAND_PLUS) % p.getnread(STRAND_MINUS)
    % (p.getnread(STRAND_BOTH)*100/static_cast<double>(mapfile.genome.getnread(STRAND_BOTH)));

  std::vector<Strand> vstr = {STRAND_BOTH, STRAND_PLUS, STRAND_MINUS};
  for (auto strand: vstr) printr(out, p.getnread_nonred(strand), p.getnread(strand));
  for (auto strand: vstr) printr(out, p.getnread_red(strand),    p.getnread(strand));

  /* reads after GCnorm */
  if(values.count("genome")) {
    for (auto strand: vstr) printr(out, p.getnread_afterGC(strand), p.getnread(strand));
  }
  out << boost::format("%1$.3f\t") % p.getdepth();
  if(p.getweight4rpm()) out << boost::format("%1$.3f\t") % p.getweight4rpm();
  else                  out << " - \t";
  if(values["ntype"].as<std::string>() == "NONE") out << p.getnread_nonred(STRAND_BOTH) << "\t";
  else out << p.getnread_rpm(STRAND_BOTH) << "\t";
  
  p.printGcov(out, mapfile.islackOfRead4GenomeCov());
  
  p.ws.printPoispar(out);
  if(values.count("bed")) out << boost::format("%1$.3f\t") % p.getFRiP();

  p.ws.printZINBpar(out);
  
  out << std::endl;
  return;
}

void output_stats(const MyOpt::Variables &values, const Mapfile &p)
{
  std::string filename = p.getbinprefix() + ".csv";
  std::ofstream out(filename);

  out << "parse2wig version " << VERSION << std::endl;
  out << "Input file: \"" << values["input"].as<std::string>() << "\"" << std::endl;
  out << "Redundancy threshold: >" << p.getthre4filtering() << std::endl;

  p.printComplexity(out);
  p.printFlen(values, out);
  if(values.count("genome")) out << "GC summit: " << p.getmaxGC() << std::endl;

  // Global stats
  out << "\n\tlength\tmappable base\tmappability\t";
  out << "total reads\t\t\t\t";
  out << "nonredundant reads\t\t\t";
  out << "redundant reads\t\t\t";
  if(values.count("genome")) out << "reads (GCnormed)\t\t\t";
  out << "read depth\t";
  out << "scaling weight\t";
  out << "normalized read number\t";
  out << "gcov (Raw)\tgcov (Normed)\t";
  out << "bin mean\tbin variance\t";
  if(values.count("bed")) out << "FRiP\t";
  out << "nb_p\tnb_n\tnb_p0\t";
  out << std::endl;
  out << "\t\t\t\t";
  out << "both\tforward\treverse\t% genome\t";
  out << "both\tforward\treverse\t";
  out << "both\tforward\treverse\t";
  if(values.count("genome")) out << "both\tforward\treverse\t";
  out << std::endl;

  // SeqStats
  print_SeqStats(values, out, p.genome, p);
  for(auto x:p.genome.chr) print_SeqStats(values, out, x, p);
  
  std::cout << "stats is output in " << filename << "." << std::endl;

  return;
}

std::vector<int8_t> makeGcovArray(const MyOpt::Variables &values, SeqStats &chr, Mapfile &p, double r4cmp)
{
  std::vector<int8_t> array;
  if(values.count("mp")) array = readMpbl_binary(values["mp"].as<std::string>(), ("chr" + p.lchr->getname()), chr.getlen());
  else array = readMpbl_binary(chr.getlen());
  if(values.count("bed")) arraySetBed(array, chr.getname(), p.genome.getvbedref());

  int32_t val(0);
  int32_t size = array.size();
  for (int32_t strand=0; strand<STRANDNUM; ++strand) {
    const std::vector<Read> &vReadref = chr.getvReadref((Strand)strand);
    for (auto &x: vReadref) {
      if(x.duplicate) continue;
      
      if(rand() >= r4cmp) val=COVREAD_ALL; else val=COVREAD_NORM;
      
      int32_t s(std::max(0, std::min(x.F3, x.F5)));
      int32_t e(std::min(std::max(x.F3, x.F5), size-1));
      if(s >= size || e < 0) {
	std::cerr << "Warning: " << chr.getname() << " read " << s <<"-"<< e << " > array size " << array.size() << std::endl;
      }
      for(int32_t i=s; i<=e; ++i) if(array[i]==MAPPABLE) array[i]=val;
    }
  }
  return array;
}

void calcGcovchr(const MyOpt::Variables &values, Mapfile &p, int32_t s, int32_t e, double r4cmp, boost::mutex &mtx)
{
  for(int32_t i=s; i<=e; ++i) {
    std::cout << p.genome.chr[i].getname() << ".." << std::flush;
    auto array = makeGcovArray(values, p.genome.chr[i], p, r4cmp);
    p.genome.chr[i].calcGcov(array);
    p.genome.addGcov(i, mtx);
  }
}

void calcGenomeCoverage(const MyOpt::Variables &values, Mapfile &p)
{
  std::cout << "calculate genome coverage.." << std::flush;

  // ignore peak region
  double r = numGcov/static_cast<double>(p.genome.getnread_nonred(STRAND_BOTH) - p.genome.getnread_inbed());
  if(r>1){
    std::cerr << "Warning: number of reads is < "<< static_cast<int>(numGcov/NUM_1M) << " million.\n";
    p.lackOfRead4GenomeCov_on();
  }
  double r4cmp = r*RAND_MAX;

  boost::thread_group agroup;
  boost::mutex mtx;
  for(uint i=0; i<p.genome.vsepchr.size(); i++) {
    agroup.create_thread(bind(calcGcovchr, boost::cref(values), boost::ref(p), p.genome.vsepchr[i].s, p.genome.vsepchr[i].e, r4cmp, boost::ref(mtx)));
  }
  agroup.join_all();
  
  std::cout << "done." << std::endl;
  return;
}

void output_wigstats(Mapfile &p)
{
  std::string filename = p.getbinprefix() + ".binarray_dist.csv";
  std::ofstream out(filename);

  std::cout << "generate " << filename << ".." << std::flush;

  out << "\tGenome\t\t\t";
  for (auto x:p.genome.chr) out << x.getname() << "\t\t\t\t";
  out << std::endl;
  out << "read number\tnum of bins genome\tprop\tZINB estimated\t";
  for (auto x:p.genome.chr) out << "num of bins\tprop\tPoisson estimated\tZINB estimated\t";
  out << std::endl;

  for(size_t i=0; i<p.genome.ws.wigDist.size(); ++i) {
    out << i << "\t";
    p.genome.ws.printwigDist(out, i);
    out << p.genome.ws.getZINB(i) << "\t";
    //    out << p.genome.getZIP(i) << "\t";
    for (auto x:p.genome.chr) {
      x.ws.printwigDist(out, i);
      out << x.ws.getPoisson(i) << "\t";
      out << x.ws.getZINB(i) << "\t";
    }
    out << std::endl;
  }

  std::cout << "done." << std::endl;
  return;
}
