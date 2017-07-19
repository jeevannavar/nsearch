#pragma once

#include <sparsepp/spp.h>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <memory>

#include "Sequence.h"
#include "Utils.h"
#include "Aligner.h"
#include "SpacedSeeds.h"

#include "Alignment/Seed.h"
#include "Alignment/Ranges.h"
#include "Alignment/HitTracker.h"
#include "Alignment/ExtendAlign.h"
#include "Alignment/BandedAlign.h"

// HSP: first and last character in sequence (i.e. seq[a1] - seq[a2])
class HSP {
public:
  size_t a1, a2;
  size_t b1, b2;
  Cigar cigar;

  HSP( size_t a1, size_t a2, size_t b1, size_t b2 )
    : a1( a1 ), a2( a2 ), b1( b1 ), b2( b2 )
  {
    assert( a2 >= a1 && b2 >= b1 );
  }

  size_t Length() const {
    return std::max( a2 - a1, b2 - b1 ) + 1;
  }

  bool IsOverlapping( const HSP &other ) const {
    return
      ( a1 <= other.a2 && other.a1 <= a2 ) // overlap in A direction
      ||
      ( b1 <= other.b2 && other.b1 <= b2 ); // overlap in B direction
  }

  size_t DistanceTo( const HSP &other ) const {
    size_t x = ( a1 > other.a2 ? a1 - other.a2 : other.a1 - a2 );
    size_t y = ( b1 > other.b2 ? b1 - other.b2 : other.b2 - b2 );

    return sqrt( x*x + y*y );
  }

  bool operator<( const HSP &other ) const {
    return Length() < other.Length();
  }
};

bool PrintWholeAlignment( const Sequence &query, const Sequence &target, const Cigar &cigar_ ) {
  std::string q;
  std::string t;
  std::string a;
  bool correct = true;

  size_t queryStart = 0;
  size_t targetStart = 0;

  Cigar cigar = cigar_;

  // Dont take left terminal gap into account
  if( !cigar.empty() ) {
    const CigarEntry &fce = cigar.front();
    if( fce.op == CigarOp::DELETION ) {
      targetStart = fce.count;
      cigar.pop_front();
    } else if( fce.op == CigarOp::INSERTION ) {
      queryStart = fce.count;
      cigar.pop_front();
    }
  }

  // Don't take right terminal gap into account
  if( !cigar.empty() ) {
    const CigarEntry &bce = cigar.back();
    if( bce.op == CigarOp::DELETION ) {
      cigar.pop_back();
    } else if( bce.op == CigarOp::INSERTION ) {
      cigar.pop_back();
    }
  }

  bool match;
  size_t numMatches = 0;
  size_t numCols = 0;

  size_t qcount = queryStart;
  size_t tcount = targetStart;

  for( auto &c : cigar ) {
    for( int i = 0; i < c.count; i++ ) {
      switch( c.op ) {
        case CigarOp::INSERTION:
          t += '-';
          q += query[ qcount++ ];
          a += ' ';
          break;

        case CigarOp::DELETION:
          q += '-';
          t += target[ tcount++ ];
          a += ' ';
          break;

        case CigarOp::MATCH:
          numMatches++;
          q += query[ qcount++ ];
          t += target[ tcount++ ];
          {
            bool match = DoNucleotidesMatch( q.back(), t.back() );
            if( !match ) {
              correct = false;
              a += 'X';
            } else {
              a += '|';
            }
          }
          break;

        case CigarOp::MISMATCH:
          a += ' ';
          q += query[ qcount++ ];
          t += target[ tcount++ ];
          break;

        default:
          break;
      }

      numCols++;
    }
  }

  std::cout << std::endl;
  std::cout << "Query " << std::string( 11, ' ' ) << ">" << query.identifier << std::endl;
  std::cout << std::endl;
  std::cout << std::setw( 15 ) << queryStart + 1 << " " << q << " " << qcount << std::endl;
  std::cout << std::string( 16, ' ' ) << a << std::endl;
  std::cout << std::setw( 15 ) << targetStart + 1 << " " << t << " " << tcount << std::endl;
  std::cout << std::endl;
  std::cout << "Target " << std::string( 11, ' ' ) << ">" << target.identifier << std::endl;

  std::cout << std::endl;
  float identity = float( numMatches ) / float( numCols );
  std::cout <<  numCols << " cols, " << numMatches << " ids (" << ( 100.0f * identity ) << "%)" << std::endl;

  std::cout << std::endl;
  std::cout << std::string( 50, '=') << std::endl;

  return correct;
}

class Database {

  using SequenceInfo =  struct {
    size_t seqIdx;
    size_t pos;
  };

  using SequenceMappingDatabase = std::unordered_map< size_t, std::vector< SequenceInfo > >;

  using WordInfo = struct {
    uint32_t index;
    uint32_t pos;
  };
  using WordInfoDatabase = std::vector< WordInfo >;

  float CalculateIdentity( const Cigar &cigar ) const {
    size_t cols = 0;
    size_t matches = 0;

    for( const CigarEntry &c : cigar ) {
      // Don't count terminal gaps towards identity calculation
      if( &c == &(*cigar.cbegin()) && ( c.op == CigarOp::INSERTION || c.op == CigarOp::DELETION ) )
        continue;
      if( &c == &(*cigar.crbegin()) && ( c.op == CigarOp::INSERTION || c.op == CigarOp::DELETION ) )
        continue;

      cols += c.count;
      if( c.op == CigarOp::MATCH )
        matches += c.count;
    }

    return cols > 0 ? float( matches ) / float( cols ) : 0.0f;
  }

public:

  void Stats() const {
    for( int i = 0; i < mNumUniqueWords; i++ ) {
      std::cout << i << "," << mWordCounts[ i ] << std::endl;
    }
  }

  Database( const SequenceList &sequences, size_t wordSize )
    : mSequences( sequences ), mWordSize( wordSize )
  {
    mNumUniqueWords = 1 << ( 2 * mWordSize ); // 2 bits per nt

    std::vector< std::vector< uint32_t > > candidatesByWord( mNumUniqueWords );

    mTotalNucleotides = 0;
    mTotalWords = 0;

    // Calculate counts
    mWordCounts.reserve( mNumUniqueWords );
    for( size_t idx = 0; idx < mSequences.size(); idx++ ) {
      const Sequence &seq = mSequences[ idx ];
      mTotalNucleotides += seq.Length();

      SpacedSeeds spacedSeeds( seq, mWordSize );
      spacedSeeds.ForEach( [&]( size_t pos, size_t word ) {
        mTotalWords++;
        mWordCounts[ word ]++;
      });
    }

    // Calculate indices
    mWordIndices.reserve( mNumUniqueWords );
    for( size_t i = 0; i < mNumUniqueWords; i++ ) {
      mWordIndices[ i ] = i > 0 ? mWordIndices[ i - 1 ] + mWordCounts[ i - 1 ] : 0;
    }

    // Now populate the word info db
    // Reset word counts, we count again from 0
    mWordCounts = std::vector< uint32_t >( mNumUniqueWords );

    mWordInfoDB.reserve( mTotalWords );
    for( size_t idx = 0; idx < mSequences.size(); idx++ ) {
      const Sequence &seq = mSequences[ idx ];

      SpacedSeeds spacedSeeds( seq, mWordSize );
      spacedSeeds.ForEach( [&]( size_t pos, size_t word ) {
        auto wordIndex = mWordIndices[ word ];
        auto wordCount = mWordCounts[ word ]++;

        auto &wordInfo = mWordInfoDB[ wordIndex + wordCount ];

        wordInfo.index = idx;
        wordInfo.pos = pos;
      });
    }
  }

  SequenceList Query( const Sequence &query, float minIdentity, int maxHits = 1, int maxRejects = 8 ) {
    const size_t defaultMinHSPLength = 16;
    const size_t maxHSPJoinDistance = 16;

    std::cout << "===> SEARCH " << query.identifier << std::endl;

    size_t minHSPLength = std::min( defaultMinHSPLength, query.Length() / 2 );

    // Go through each kmer, find hits
    if( mHits.size() < mSequences.size() ) {
      mHits.resize( mSequences.size() );
    }

    // Fast counter reset
    memset( mHits.data(), 0, sizeof( size_t ) * mHits.capacity() );

    class Candidate {
    public:
      size_t seqIdx;
      size_t counter;

      Candidate( size_t seqIdx, size_t counter )
        : seqIdx( seqIdx ), counter( counter )
      {
      }

      bool operator<( const Candidate &other ) const {
        return counter < other.counter;
      }
    };

    std::multiset< Candidate > highscores;
    SpacedSeeds spacedSeeds( query, mWordSize );
    spacedSeeds.ForEach( [&]( size_t pos, uint32_t word ) {
      for( uint32_t i = 0; i < mWordCounts[ word ]; i++ ) {
        auto &wordInfo = mWordInfoDB[ mWordIndices[ word ] + i ];

        size_t candidateIdx = wordInfo.index;
        size_t candidatePos = wordInfo.pos;

        const Sequence &candidateSeq = mSequences[ candidateIdx ];

        size_t &counter = mHits[ candidateIdx ];
        counter++;

        if( highscores.empty() || counter > highscores.begin()->counter ) {
          auto it = std::find_if( highscores.begin(), highscores.end(), [candidateIdx]( const Candidate &c ) {
            return c.seqIdx == candidateIdx;
          });

          if( it != highscores.end() ) {
            highscores.erase( it );
          }

          highscores.insert( Candidate( candidateIdx, counter ) );
          if( highscores.size() > maxHits + maxRejects ) {
            highscores.erase( highscores.begin() );
          }
        }
      }
    });

    // For each candidate:
    // - Get HSPs,
    // - Check for good HSP (>= similarity threshold)
    // - Join HSP together
    // - Align
    // - Check similarity
    int numHits = 0;
    int numRejects = 0;
    std::cout << "Highscores " << highscores.size() << std::endl;
    for( auto it = highscores.rbegin(); it != highscores.rend(); ++it ) {
      const size_t seqIdx = (*it).seqIdx;
      assert( seqIdx < mSequences.count() );
      const Sequence &candidateSeq = mSequences[ seqIdx ];
      std::cout << "Highscore Entry " << seqIdx << " " << (*it).counter << std::endl;

      // Go through each kmer, find hits
      HitTracker hitTracker;
      spacedSeeds.ForEach( [&]( size_t pos, uint32_t word ) {
        for( uint32_t i = 0; i < mWordCounts[ word ]; i++ ) {
          auto &wordInfo = mWordInfoDB[ mWordIndices[ word ] + i ];

          size_t candidateIdx = wordInfo.index;
          size_t candidatePos = wordInfo.pos;

          if( candidateIdx != seqIdx )
            continue;

          hitTracker.AddHit( pos, candidatePos, mWordSize );
        }
      });

      // Find all HSP
      // Sort by length
      // Try to find best chain
      // Fill space between with banded align

      std::set< HSP > hsps;
      for( auto &seed : hitTracker.Seeds() ) {
        size_t queryPos, candidatePos;


        size_t a1 = seed.s1, a2 = seed.s1 + seed.length - 1,
               b1 = seed.s2, b2 = seed.s2 + seed.length - 1;

        Cigar leftCigar;
        int leftScore = mExtendAlign.Extend( query, candidateSeq,
            &queryPos, &candidatePos,
            &leftCigar,
            AlignmentDirection::backwards,
            a1, b1 );
        if( !leftCigar.empty() ) {
          a1 = queryPos;
          b1 = candidatePos;
        }

        Cigar rightCigar;
        size_t rightQuery, rightCandidate;
        int rightScore = mExtendAlign.Extend( query, candidateSeq,
            &queryPos, &candidatePos,
            &rightCigar,
            AlignmentDirection::forwards,
            a2 + 1, b2 + 1 );
        if( !rightCigar.empty() ) {
          a2 = queryPos;
          b2 = candidatePos;
        }

        HSP hsp( a1, a2, b1, b2 );
        if( hsp.Length() >= minHSPLength ) {
          // Construct hsp cigar (spaced seeds so we cannot assume full match)
          Cigar middleCigar;
          for( size_t s1 = seed.s1, s2 = seed.s2;
               s1 < seed.s1 + seed.length && s2 < seed.s2 + seed.length;
               s1++, s2++ )
          {
            bool match = DoNucleotidesMatch( query[ s1 ], candidateSeq[ s2 ] );
            middleCigar.Add( match ? CigarOp::MATCH : CigarOp::MISMATCH );
          }
          hsp.cigar = leftCigar + middleCigar + rightCigar ;

          // Save HSP
          hsps.insert( hsp );
        }
      }

      // Greedy join HSPs if close
      struct HSPChainOrdering {
        bool operator() ( const HSP &left, const HSP &right ) const {
          return left.a1 < right.a1 && left.b1 < right.b1;
        }
      };

      std::set< HSP, HSPChainOrdering > chain;
      for( auto it = hsps.rbegin(); it != hsps.rend(); ++it ) {
        const HSP &hsp = *it;
        bool hasNoOverlaps = std::none_of( chain.begin(), chain.end(), [&]( const HSP &existing ) {
          return hsp.IsOverlapping( existing );
        });
        if( hasNoOverlaps ) {
          bool anyHSPJoinable = std::any_of( chain.begin(), chain.end(), [&]( const HSP &existing ) {
            return hsp.DistanceTo( existing ) <= maxHSPJoinDistance;
          });

          if( chain.empty() || anyHSPJoinable ) {
            chain.insert( hsp );
          }
        }
      }

      bool accept = false;
      if( chain.size() > 0 ) {
        Cigar alignment;
        Cigar cigar;

        // Align first HSP's start to whole sequences begin
        auto &first = *chain.cbegin();
        mBandedAlign.Align( query, candidateSeq, &cigar, AlignmentDirection::backwards, first.a1, first.b1 );
        alignment += cigar;

        // Align in between the HSP's
        for( auto it1 = chain.cbegin(), it2 = ++chain.cbegin();
            it1 != chain.cend() && it2 != chain.cend();
            ++it1, ++it2 )
        {
          auto &current = *it1;
          auto &next = *it2;

          alignment += current.cigar;
          mBandedAlign.Align( query, candidateSeq, &cigar,
              AlignmentDirection::forwards,
              current.a2 + 1, current.b2 + 1,
              next.a1, next.b1 );
          alignment += cigar;
        }

        // Align last HSP's end to whole sequences end
        auto &last = *chain.crbegin();
        alignment += last.cigar;
        mBandedAlign.Align( query, candidateSeq, &cigar, AlignmentDirection::forwards, last.a2 + 1, last.b2 + 1 );
        alignment += cigar;

        float identity = CalculateIdentity( alignment );
        if( identity >= minIdentity ) {
          accept = true;
          bool correct = PrintWholeAlignment( query, candidateSeq, alignment );
          if( !correct ) {
            std::cout << "INVALID ALIGNMENT" << std::endl;
          }
        }
      }

      if( accept ) {
        numHits++;
        if( numHits >= maxHits )
          break;
      } else {
        numRejects++;
        if( numRejects >= maxRejects )
          break;
      }
    }

    return SequenceList();
  }

private:
  ExtendAlign mExtendAlign;
  BandedAlign mBandedAlign;

  size_t mWordSize;

  std::vector< size_t > mHits;
  SequenceMappingDatabase mWordDB;

  SequenceList mSequences;
  size_t mTotalNucleotides;
  size_t mTotalWords;
  size_t mNumUniqueWords;

  std::vector< uint32_t > mWordIndices;
  std::vector< uint32_t > mWordCounts;
  WordInfoDatabase mWordInfoDB;
};
