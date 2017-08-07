#include <docopt.h>
#include <functional>
#include <iostream>
#include <sstream>
#include <utility>

#include <nsearch/FASTA/Reader.h>
#include <nsearch/Sequence.h>
#include <nsearch/Alphabet/DNA.h>
#include <nsearch/Alphabet/Protein.h>

#include "Common.h"
#include "Merge.h"
#include "Search.h"
#include "Stats.h"

Stats gStats;

static const char USAGE[] = R"(
  Process and search sequences.

  Usage:
    nsearch search (dna|protein) --query=<queryfile> --database=<databasefile>
      --out=<outputfile> --minidentity=<minidentity> [--maxaccepts=<maxaccepts>] [--maxrejects=<maxrejects>]
    nsearch merge --forward=<forwardfile> --reverse=<reversefile> --out=<outputfile>

  Options:
    --minidentity=<minidentity>    Minimum identity threshold (e.g. 0.8).
    --maxaccepts=<maxaccepts>      Maximum number of successful hits reported for one query [default: 1].
    --maxrejects=<maxrejects>      Abort after this many candidates were rejected [default: 8].
)";

void PrintSummaryHeader() {
  std::cout << std::endl << std::endl;
  std::cout << "Summary:" << std::endl;
}

void PrintSummaryLine( const float value, const std::string& line,
                       const float    total = 0.0,
                       const UnitType unit  = UnitType::COUNTS ) {
  std::ios::fmtflags f( std::cout.flags() );
  std::cout << std::setw( 10 ) << ValueWithUnit( value, unit );
  std::cout << ' ' << line;
  if( total > 0.0 ) {
    std::cout << " (" << value / total * 100.0 << "%)";
  }
  std::cout << std::endl;
  std::cout.flags( f );
}

int main( int argc, const char** argv ) {
  std::map< std::string, docopt::value > args =
    docopt::docopt( USAGE, { argv + 1, argv + argc },
                    true, // help
                    APP_NAME );

  // Print header
  std::cout << APP_NAME << " " << APP_VERSION << " (built on "
            << BUILD_TIMESTAMP << ")" << std::endl;

  // Search
  if( args[ "search" ].asBool() ) {
    gStats.StartTimer();

    auto query      = args[ "--query" ].asString();
    auto db         = args[ "--database" ].asString();
    auto out        = args[ "--out" ].asString();
    auto minid      = std::stof( args[ "--minidentity" ].asString() );
    auto maxaccepts = args[ "--maxaccepts" ].asLong();
    auto maxrejects = args[ "--maxrejects" ].asLong();

    if( args[ "dna" ].asBool() ) {
      Search< DNA >( query, db, out, minid, maxaccepts, maxrejects );
    } else if( args[ "protein" ].asBool() ) {
      Search< Protein >( query, db, out, minid, maxaccepts, maxrejects );
    }

    gStats.StopTimer();

    PrintSummaryHeader();
    PrintSummaryLine( gStats.ElapsedMillis() / 1000.0, "Seconds" );
  }

  // Merge
  if( args[ "merge" ].asBool() ) {
    gStats.StartTimer();

    Merge( args[ "--forward" ].asString(), args[ "--reverse" ].asString(),
           args[ "--out" ].asString() );

    gStats.StopTimer();

    PrintSummaryHeader();
    PrintSummaryLine( gStats.ElapsedMillis() / 1000.0, "Seconds" );
    PrintSummaryLine( gStats.numProcessed / gStats.ElapsedMillis(),
                      "Processed/ms" );
    PrintSummaryLine( gStats.numProcessed, "Pairs" );
    PrintSummaryLine( gStats.numMerged, "Merged", gStats.numProcessed );
    PrintSummaryLine( gStats.MeanMergedLength(), "Mean merged length" );
  }

  return 0;
}
