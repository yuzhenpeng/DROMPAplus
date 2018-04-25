/* Copyright(c) Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */

#include "dd_command.hpp"

//             PC_BROAD    peak-calling (for broad mode)
//             3DMAP       accumulate read counts in bed regions specified

std::vector<Command> generateCommands()
{
  std::vector<Command> cmds;
  // PC_SHARP
  cmds.emplace_back(Command("PC_SHARP", "Visualization: for sharp mark parameter set",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::THRE, DrompaCommand::ANNO_PC, DrompaCommand::ANNO_GV, DrompaCommand::DRAW, DrompaCommand::REGION, DrompaCommand::OTHER},
			    CommandParamSet(5, 1, 0, 30, 4, 5, true)));
  cmds.emplace_back(Command("PC_BROAD", "Visualization: for broad mark parameter set",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::THRE, DrompaCommand::ANNO_PC, DrompaCommand::ANNO_GV, DrompaCommand::DRAW, DrompaCommand::REGION, DrompaCommand::OTHER},
			    CommandParamSet(10, 1, 0, 30, 4, 5, true)));
  // PC_ENRICH 
  cmds.emplace_back(Command("PC_ENRICH","Visualization: ChIP/Input enrichment",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::THRE, DrompaCommand::ANNO_PC, DrompaCommand::ANNO_GV, DrompaCommand::DRAW, DrompaCommand::REGION, DrompaCommand::OTHER},
			    CommandParamSet(5, 0, 1, 30, 4, 5, false)));
  // GV
  cmds.emplace_back(Command("GV", "Visualization: global-view enrichment",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_GV,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::ANNO_GV, DrompaCommand::DRAW, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 1, 2000, 1, 10, false)));
  // PD
  cmds.emplace_back(Command("PD", "peak density",
			    "-pd <pdfile>,<name> [-pd <pdfile>,<name> ...]",
			    //	   dd_pd,
			    exec_PCSHARP,
			    {DrompaCommand::PD, DrompaCommand::ANNO_GV, DrompaCommand::DRAW, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 0, 0, 0, false)));
  cmds.emplace_back(Command("CI", "compare peak-intensity between two samples",
			    "-i <ChIP>,,<name> -i <ChIP>,,<name> -bed <bedfile>",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 0, 0, 0, false)));
  cmds.emplace_back(Command("PROFILE", "make R script of averaged read density",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::PROF, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 0, 0, 0, false)));
  cmds.emplace_back(Command("HEATMAP", "make heatmap of multiple samples",
			    "-i <ChIP>,<Input>,<name> [-i <ChIP>,<Input>,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::NORM, DrompaCommand::PROF, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 30, 4, 5, false)));
  cmds.emplace_back(Command("CG", "output ChIP-reads in each gene body",
			    "-i <ChIP>,,<name> [-i <ChIP>,,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::CG, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 0, 0, 0, false)));
  cmds.emplace_back(Command("TR",      "calculate the travelling ratio (pausing index) for each gene",
			    "-i <ChIP>,,<name> [-i <ChIP>,,<name> ...]",
			    exec_PCSHARP,
			    {DrompaCommand::CHIP, DrompaCommand::PROF, DrompaCommand::OTHER},
			    CommandParamSet(0, 0, 0, 0, 0, 0, false)));
  cmds.emplace_back(Command("GOVERLOOK", "genome-wide overlook of peak positions",
			    "-bed <bedfile>,<name> [-bed <bedfile>,<name> ...]",
			    //dd_overlook,
			    exec_PCSHARP,
			    {DrompaCommand::OTHER},
			    CommandParamSet(5, 0, 0, 0, 0, 0, false)));
  return cmds;
}

void Command::InitDump()
{
  std::vector<std::string> str_bool = {"OFF", "ON"};
  std::vector<std::string> str_stype = { "ChIP read", "Enrichment ratio", "Enrichment P-value" };
  std::vector<std::string> str_ptype = { "NONE", "TSS", "TTS", "GENE100", "SPECIFIEDSITES" };
  std::vector<std::string> str_ntype = { "WHOLE GENOME", "TARGET REGIONS ONLY" };

  std::cout << boost::format("\n======================================\n");
  std::cout << boost::format("drompa+ version %1%: %2%\n\n") % VERSION % name;
  std::cout << boost::format("output prefix: %1%\n")     % MyOpt::getVal<std::string>(values, "output");
  std::cout << boost::format("Genome-table file: %1%\n") % MyOpt::getVal<std::string>(values, "gt");

  for (auto x: vopts) {
    switch(x) {
    case DrompaCommand::CHIP:    p.InitDumpChIP(); break;
    case DrompaCommand::NORM:    p.InitDumpNorm(); break;
    case DrompaCommand::THRE:    p.thre.InitDump(); break;
    case DrompaCommand::ANNO_PC: p.anno.InitDumpPC(values); break;
    case DrompaCommand::ANNO_GV: p.anno.InitDumpGV(values); break;
    case DrompaCommand::DRAW:    p.drawparam.InitDump(); break;
    case DrompaCommand::REGION:  p.drawregion.InitDump(values); break;
    case DrompaCommand::CG: 
      {
	DEBUGprint("INITDUMP:DrompaCommand::CG");
	//	for (auto x: {"cgthre"}) chkminus<int32_t>(values, x, -1);
	break;
      }
    case DrompaCommand::TR: 
      {
	DEBUGprint("INITDUMP:DrompaCommand::TR");
	//	for (auto x: {"tssthre"}) chkminus<int32_t>(values, x, -1);
	break;
      }
    case DrompaCommand::PD:
      {
	DEBUGprint("INITDUMP:DrompaCommand::PD");
	std::cout << boost::format("\nSamples\n");
	for(uint i=0; i<p.pd.size(); ++i) {
	  std::cout << boost::format("   IP%1%: %2%\tname: %3%\n") % (i+1) % p.pd[i].argv % p.pd[i].name;
	}
	break;
      }
    case DrompaCommand::PROF:
      {
	DEBUGprint("INITDUMP:DrompaCommand::PROF");
	std::cout << boost::format("   show type: %1$\n")             % str_stype[MyOpt::getVal<int32_t>(values, "stype")];
	std::cout << boost::format("   profile type: %1$\n")          % str_ptype[MyOpt::getVal<int32_t>(values, "ptype")];
	std::cout << boost::format("   profile normalization: %1$\n") % str_ntype[MyOpt::getVal<int32_t>(values, "ntype")];
	break;
      }
    case DrompaCommand::OTHER: p.InitDumpOther(); break;
    }
  }
  
  printf("======================================\n");
  return;
}

