#include "Search.h"

#include <nsearch/Sequence.h>
#include <nsearch/FASTA/Reader.h>
#include <nsearch/Alnout/Writer.h>
#include <nsearch/Database.h>
#include <nsearch/Database/GlobalSearch.h>

#include "WorkerQueue.h"

#include "Common.h"

using QueryWithHits = std::pair< Sequence, GlobalSearch::HitList >;
using QueryWithHitsList = std::deque< QueryWithHits >;

template<>
class QueueItemInfo< QueryWithHitsList > {
public:
  static size_t Count( const QueryWithHitsList &list ) { return list.size(); }
};

class SearchResultsWriterWorker {
public:
  SearchResultsWriterWorker( const std::string &path )
    : mWriter( path )
  {

  }

  void Process( const QueryWithHitsList &queryWithHitsList ) {
    for( auto &queryWithHits : queryWithHitsList ) {
      mWriter << queryWithHits;
    }
  }

private:
  Alnout::Writer mWriter;
};
using SearchResultsWriter = WorkerQueue< SearchResultsWriterWorker, QueryWithHitsList, const std::string& >;

template<>
class QueueItemInfo< SequenceList > {
public:
  static size_t Count( const SequenceList &list ) { return list.size(); }
};

class QueryDatabaseSearcherWorker {
public:
  QueryDatabaseSearcherWorker( SearchResultsWriter &writer, const Database &database )
    : mWriter( writer), mGlobalSearch( database, 0.75, 1, 8 )
  {
  }

  void Process( const SequenceList &queries ) {
    QueryWithHitsList list;

    for( auto &query : queries ) {
      auto hits = mGlobalSearch.Query( query );
      if( hits.empty() )
        continue;

      list.push_back( { query, hits } );
    }

    if( !list.empty() ) {
      mWriter.Enqueue( list );
    }
  }

private:
  GlobalSearch mGlobalSearch;
  SearchResultsWriter &mWriter;
};
using QueryDatabaseSearcher = WorkerQueue< QueryDatabaseSearcherWorker, SequenceList, SearchResultsWriter&, const Database& >;

bool Search( const std::string &queryPath, const std::string &databasePath, const std::string &outputPath ) {
  ProgressOutput progress;

  Sequence seq;
  SequenceList sequences;

  FASTA::Reader dbReader( databasePath );

  enum ProgressType { ReadDBFile, StatsDB, IndexDB, ReadQueryFile, SearchDB, WriteHits };

  progress.Add( ProgressType::ReadDBFile, "Reading DB file", UnitType::BYTES );
  progress.Add( ProgressType::StatsDB, "Analyzing DB sequences");
  progress.Add( ProgressType::IndexDB, "Indexing database");
  progress.Add( ProgressType::ReadQueryFile, "Reading query file", UnitType::BYTES );
  progress.Add( ProgressType::SearchDB, "Searching database" );
  progress.Add( ProgressType::WriteHits, "Write hits" );

  // Read DB
  progress.Activate( ProgressType::ReadDBFile );
  while( !dbReader.EndOfFile() ) {
    dbReader >> seq;
    sequences.push_back( std::move( seq ) );
    progress.Set( ProgressType::ReadDBFile, dbReader.NumBytesRead(), dbReader.NumBytesTotal() );
  }

  // Index DB
  auto dbCallback = [&]( Database::ProgressType type, size_t num, size_t total ) {
    switch( type ) {
      case Database::ProgressType::StatsCollection:
        progress.Activate( ProgressType::StatsDB ).Set( ProgressType::StatsDB, num, total );
        break;

      case Database::ProgressType::Indexing:
        progress.Activate( ProgressType::IndexDB ).Set( ProgressType::IndexDB, num, total );
        break;

      default:
        break;
    }
  };
  Database db( sequences, 8, dbCallback );

  // Read and process queries
  const int numQueriesPerWorkItem = 50;

  SearchResultsWriter writer( 1, outputPath );
  QueryDatabaseSearcher searcher( -1, writer, db );

  searcher.OnProcessed( [&]( size_t numProcessed, size_t numEnqueued ) {
    progress.Set( ProgressType::SearchDB, numProcessed, numEnqueued );
  });

  writer.OnProcessed( [&]( size_t numProcessed, size_t numEnqueued ) {
    progress.Set( ProgressType::WriteHits, numProcessed, numEnqueued );
  });

  FASTA::Reader qryReader( queryPath );

  SequenceList queries;
  progress.Activate( ProgressType::ReadQueryFile );
  while( !qryReader.EndOfFile() )  {
    qryReader.Read( queries, numQueriesPerWorkItem );
    searcher.Enqueue( queries );
    progress.Set( ProgressType::ReadQueryFile, qryReader.NumBytesRead(), qryReader.NumBytesTotal() );
  }

  // Search
  progress.Activate( ProgressType::SearchDB );
  searcher.WaitTillDone();

  progress.Activate( ProgressType::WriteHits );
  writer.WaitTillDone();

  return true;
}