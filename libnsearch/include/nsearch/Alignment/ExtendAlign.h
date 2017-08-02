#pragma once

#include "Cigar.h"
#include "Common.h"

#include <cassert>
#include <iostream>
#include <vector>

typedef struct {
  int xDrop = 32;

  int matchScore    = 2;
  int mismatchScore = -4;

  int gapOpenScore   = -20;
  int gapExtendScore = -2;
} ExtendAlignParams;

typedef struct {
  int   bestA, bestB;
  Cigar cigar;
} ExtendedAlignment;

// Influenced by Blast's SemiGappedAlign function
template < typename Alphabet >
class ExtendAlign {
private:
  struct Cell {
    int score    = MININT;
    int scoreGap = MININT;
  };
  using Cells = std::vector< Cell >;

  void Print( const Cells& row ) {
    for( auto& c : row ) {
      if( c.score <= MININT ) {
        printf( "%5c", 'X' );
      } else {
        printf( "%5d", c.score );
      }
    }
    printf( "\n" );
  }

  ExtendAlignParams mAP;
  Cells             mRow;
  CigarOps          mOperations;

public:
  ExtendAlign( const ExtendAlignParams& ap = ExtendAlignParams() )
      : mAP( ap ) {}

  const ExtendAlignParams& AP() const {
    return mAP;
  }

  // Heavily influenced by Blast's SemiGappedAlign function
  int Extend( const Sequence< Alphabet >& A, const Sequence< Alphabet >& B,
              size_t* bestA = NULL, size_t* bestB = NULL, Cigar* cigar = NULL,
              const AlignmentDirection dir = AlignmentDirection::fwd,
              size_t startA = 0, size_t startB = 0 ) {
    int    score;
    size_t x, y;
    size_t aIdx, bIdx;
    size_t bestX, bestY;

    size_t width, height;

    if( dir == AlignmentDirection::fwd ) {
      width  = A.Length() - startA + 1;
      height = B.Length() - startB + 1;
    } else {
      width  = startA + 1;
      height = startB + 1;
    }

    if( mRow.capacity() < width ) {
      // Enlarge vector
      mRow = Cells( width * 1.5 );
    }

    if( mOperations.capacity() < width * height ) {
      mOperations = CigarOps( width * height * 1.5 );
    }

    bestX = 0;
    bestY = 0;

    if( bestA )
      *bestA = startA;
    if( bestB )
      *bestB = startB;

    int bestScore      = 0;
    mRow[ 0 ].score    = 0;
    mRow[ 0 ].scoreGap = mAP.gapOpenScore + mAP.gapExtendScore;

    for( x = 1; x < width; x++ ) {
      score = mAP.gapOpenScore + x * mAP.gapExtendScore;

      if( score < -mAP.xDrop )
        break;

      mOperations[ x ]   = CigarOp::INSERTION;
      mRow[ x ].score    = score;
      mRow[ x ].scoreGap = MININT;
    }
    size_t rowSize = x;
    /* Print( mRow ); */

    size_t firstX = 0;

    for( y = 1; y < height; y++ ) {

      int rowGap    = MININT;
      int score     = MININT;
      int diagScore = MININT;

      size_t lastX = firstX;

      for( x = firstX; x < rowSize; x++ ) {
        int colGap = mRow[ x ].scoreGap;

        aIdx = 0;
        bIdx = 0;
        bool match;
        if( x > 0 ) {
          // diagScore: score at col-1, row-1

          if( dir == AlignmentDirection::fwd ) {
            aIdx = startA + x - 1;
            bIdx = startB + y - 1;
          } else {
            aIdx = startA - x;
            bIdx = startB - y;
          }

          /* printf( "x:%zu y:%zu %c == %c\n", x, y, A[ aIdx ], B[ bIdx ] ); */
          match = Alphabet::Match( A[ aIdx ], B[ bIdx ] );
          score = diagScore + ( match ? mAP.matchScore : mAP.mismatchScore );
        }

        // select highest score
        //  - coming from diag (current),
        //  - coming from left (row)
        //  - coming from top (col)
        if( score < rowGap )
          score = rowGap;
        if( score < colGap )
          score = colGap;

        // mRow[ x ] right now points to the previous row, so use this
        // in the next iteration for the diagonal computation of (x, y )
        diagScore = mRow[ x ].score;

        if( bestScore - score > mAP.xDrop ) {
          // X-Drop test failed
          mRow[ x ].score = MININT;

          if( x == firstX ) {
            // Tighten left bound
            firstX++;
          }
        } else {
          lastX = x;

          // Check if we achieved new highscore
          if( score > bestScore ) {
            bestScore = score;

            if( bestA )
              *bestA = aIdx;
            if( bestB )
              *bestB = bIdx;

            bestX = x;
            bestY = y;
          }

          // Record new score
          CigarOp op;
          if( score == rowGap ) {
            op = CigarOp::INSERTION;
          } else if( score == colGap ) {
            op = CigarOp::DELETION;
          } else {
            op = match ? CigarOp::MATCH : CigarOp::MISMATCH;
          }
          mOperations[ y * width + x ] = op;

          mRow[ x ].score = score;
          mRow[ x ].scoreGap =
            std::max( score + mAP.gapOpenScore + mAP.gapExtendScore,
                      colGap + mAP.gapExtendScore );
          rowGap = std::max( score + mAP.gapOpenScore + mAP.gapExtendScore,
                             rowGap + mAP.gapExtendScore );
        }
      }

      if( firstX == rowSize ) {
        // All cells failed the X-Drop test
        // We are done 8)
        break;
      }

      if( lastX < rowSize - 1 ) {
        // Tighten right bound
        rowSize = lastX + 1;
      } else {
        // Extend row, since last checked column didn't fail X-Drop test
        while( rowGap >= ( bestScore - mAP.xDrop ) && rowSize < width ) {
          mRow[ rowSize ].score = rowGap;
          mRow[ rowSize ].scoreGap =
            rowGap + mAP.gapOpenScore + mAP.gapExtendScore;
          mOperations[ y * width + rowSize ] = CigarOp::INSERTION;
          rowGap += mAP.gapExtendScore;
          rowSize++;
        }
      }

      // Properly reset right bound
      if( rowSize < width ) {
        mRow[ rowSize ].score    = MININT;
        mRow[ rowSize ].scoreGap = MININT;
        rowSize++;
      }
      /* Print( mRow ); */
    }

    if( cigar ) {
      size_t bx = bestX;
      size_t by = bestY;

      CigarEntry ce;
      cigar->Clear();
      while( bx != 0 || by != 0 ) {
        CigarOp op = mOperations[ by * width + bx ];
        cigar->Add( op );

        switch( op ) {
          case CigarOp::INSERTION:
            bx--;
            break;
          case CigarOp::DELETION:
            by--;
            break;
          case CigarOp::MATCH:
            bx--;
            by--;
            break;
          case CigarOp::MISMATCH:
            bx--;
            by--;
            break;
          default:
            assert( true );
            break;
        }
      }

      if( dir == AlignmentDirection::fwd ) {
        cigar->Reverse();
      }
    }

    return bestScore;
  };
};
