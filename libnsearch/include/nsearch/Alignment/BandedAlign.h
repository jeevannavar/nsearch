#pragma once

#include "Common.h"

typedef struct BandedAlignParams {
  size_t bandwidth = 16;

  int matchScore = 2;
  int mismatchScore = -4;

  int interiorGapOpenScore = -20;
  int interiorGapExtendScore = -2;

  int terminalGapOpenScore = -2;
  int terminalGapExtendScore = -1;
} BandedAlignParams;

class BandedAlign {
private:
  class Gap {
  private:
    int mScore;
    bool mIsTerminal;

    int mTerminalGapScore, mInteriorGapScore;
    int mTerminalGapExtendScore, mInteriorGapExtendScore;

  public:
    Gap( const BandedAlignParams &params )
      :
        mTerminalGapScore( params.terminalGapOpenScore + params.terminalGapExtendScore ),
        mTerminalGapExtendScore( params.terminalGapExtendScore ),

        mInteriorGapScore( params.interiorGapOpenScore + params.interiorGapExtendScore ),
        mInteriorGapExtendScore( params.interiorGapExtendScore )
    {
      Reset();
    }

    void OpenOrExtend( int score, bool terminal, size_t length = 1 ) {
      int newGapScore = score + length * ( terminal ? mTerminalGapScore : mInteriorGapScore );

      Extend();

      if( newGapScore > mScore ) {
        mScore = newGapScore;
        mIsTerminal = terminal;
      }
    }

    void Extend( size_t length = 1 ) {
      mScore += length * ( mIsTerminal ? mTerminalGapExtendScore : mInteriorGapExtendScore );
    }

    int Score() {
      return mScore;
    }

    void Reset() {
      mScore = MININT;
      mIsTerminal = false;
    }
  };

  using Scores = std::vector< int >;
  using Gaps = std::vector< Gap >;
  using Operations = std::vector< char >;

  void PrintRow( size_t width ) {
    for( int i = 0; i < width; i++ ) {
      int score = mScores[ i ];
      if( score <= MININT ) {
        printf( "%5c", 'X' );
      }  else {
        printf( "%5d", score );
      }
    }
    printf("\n");
  }

  Scores mScores;
  Gaps mVerticalGaps;
  Operations mOperations;
  BandedAlignParams mParams;

public:
  BandedAlign( const BandedAlignParams& params = BandedAlignParams() )
    : mParams( params )
  {

  }

  int Align( const Sequence &A, const Sequence &B,
      Cigar *cigar = NULL,
      size_t startA = 0, size_t startB = 0,
      AlignmentDirection dir = AlignmentDirection::forwards,
      size_t endA = -1, size_t endB = -1 )
  {
    // Calculate matrix width, depending on alignment
    // direction and length of sequences
    // A will be on the X axis (width of matrix)
    // B will be on the Y axis (height of matrix)
    size_t width, height;

    size_t lenA = A.Length();
    size_t lenB = B.Length();

    if( endA == ( size_t )-1 ) {
      endA = ( dir == AlignmentDirection::forwards ? lenA : 0 );
    }

    if( endB == ( size_t )-1 ) {
      endB = ( dir == AlignmentDirection::forwards ? lenB : 0 );
    }

    width = ( endA > startA ? endA - startA : startA - endA ) + 1;
    height = ( endB > startB ? endB - startB : startB - endB ) + 1;

    // Make sure we have enough cells
    if( mScores.capacity() < width ) {
      mScores = Scores( width * 1.5, MININT );
    }

    if( mVerticalGaps.capacity() < width ) {
      mVerticalGaps = Gaps( width * 1.5, mParams );
    }

    if( mOperations.capacity() < width * height ) {
      mOperations = Operations( width * height * 1.5 );
    }

    // Initialize first row
    size_t bw = mParams.bandwidth;

    bool fromBeginningA = ( startA == 0 || startA == lenA );
    bool fromBeginningB = ( startB == 0 || startB == lenB );

    bool fromEndA = ( endA == 0 || endA == lenA );
    bool fromEndB = ( endB == 0 || endB == lenB );

    mScores[ 0 ] = 0;
    mVerticalGaps[ 0 ].OpenOrExtend( mScores[ 0 ], fromBeginningB );

    Gap horizontalGap( mParams );

    size_t x, y;
    for( x = 1; x < width; x++ ) {
      if( x > bw && height > 1 ) // only break on BW bound if B is not empty
        break;

      horizontalGap.OpenOrExtend( mScores[ x - 1 ], fromBeginningA );
      mScores[ x ] = horizontalGap.Score();
      mOperations[ x ] = 'D';
    }
    if( x < width ) {
      mScores[ x ] = MININT;
      mVerticalGaps[ x ].Reset();
    }
    PrintRow( width );

    // Row by row...
    size_t center = 1;
    for( y = 1; y < height; y++ ) {
      int score = MININT;

      // Calculate band bounds
      size_t leftBound = std::min( center > bw ? ( center - bw ) : 0, width - 1 );
      size_t rightBound = std::min( center + bw, width - 1 );

      /* // If we are in the last row, make sure we calculate up to the last cell (for traceback) */
      /* if( y == height - 1 ) { */
      /*   rightBound = width - 1; */
      /* } */

      // Set diagonal score for first calculated cell in row
      int diagScore = MININT;
      if( leftBound > 0 ) {
        diagScore = mScores[ leftBound - 1 ];
        mScores[ leftBound - 1 ] = MININT;
        mVerticalGaps[ leftBound - 1].Reset();
      }

      // Calculate row within the band bounds
      horizontalGap.Reset();
      for( x = leftBound; x <= rightBound; x++ ) {
        // Calculate diagonal score
        size_t aIdx = 0, bIdx = 0;
        bool match;
        if( x > 0 ) {
          aIdx = ( dir == AlignmentDirection::forwards ) ? startA + x - 1 : startA - x;
          bIdx = ( dir == AlignmentDirection::forwards ) ? startB + y - 1 : startB - y;
          // diagScore: score at col-1, row-1
          match = DoNucleotidesMatch( A[ aIdx ], B[ bIdx ] );
          score = diagScore + ( match ? mParams.matchScore : mParams.mismatchScore );
        }

        // Select highest score
        //  - coming from diag (current),
        //  - coming from left (row)
        //  - coming from top (col)
        if( score < horizontalGap.Score() )
          score = horizontalGap.Score();

        Gap &verticalGap = mVerticalGaps[ x ];
        if( score < verticalGap.Score() )
          score = verticalGap.Score();

        // Save the prev score at (x - 1) which
        // we will use to compute the diagonal score at (x)
        diagScore = mScores[ x ];

        // Save new score
        mScores[ x ] = score;

        char op;
        if( score == horizontalGap.Score() ) {
          op = 'D';
        } else if( score == verticalGap.Score() ) {
          op = 'I';
        } else {
          op = match ? 'M' : 'X';
        }
        mOperations[ y * width + x ] = op;

        // Calculate potential gaps
        bool isTerminalA = ( x == 0 || x == width - 1 ) && fromEndA;
        bool isTerminalB = ( y == 0 || y == height - 1 ) && fromEndB;

        horizontalGap.OpenOrExtend( score, isTerminalB );
        verticalGap.OpenOrExtend( score, isTerminalA );
      }

      if( rightBound + 1 < width ) {
        mScores[ rightBound + 1 ] = MININT;
        mVerticalGaps[ rightBound + 1].Reset();
      }

      PrintRow( width );

      if( rightBound == leftBound ) {
        break;
      }

      // Move one cell over for the next row
      center++;
    }

    int score = mScores[ x - 1 ];
    std::cout << score << std::endl;

    if( x == width ) {
      // We reached the end of A, emulate going down on B (vertical gaps)
      Gap &verticalGap = mVerticalGaps[ x - 1 ];
      verticalGap.Extend( ( height - y ) );
      score = verticalGap.Score();
    }

    // Backtrack
    if( cigar ) {
      size_t x = width - 1;
      size_t y = height - 1;
      cigar->reserve( std::max( x, y ) );
      while( x != 0 || y != 0 ) {
        char op = mOperations[ y * width + x ];
        cigar->push_back( op );
        switch( op ) {
          case 'D': x--; break;
          case 'I': y--; break;
          case 'M': x--; y--; break;
          case 'X': x--; y--; break;
        }
      }

      if( dir == AlignmentDirection::forwards ) {
        std::reverse( cigar->begin(), cigar->end() );
      }
    }

    return score;
  }
};
