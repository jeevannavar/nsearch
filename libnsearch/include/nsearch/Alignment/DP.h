/* #pragma once */

/* #include <iostream> */
/* #include <iomanip> */
/* #include <stdio.h> */

/* #include <set> */
/* #include <deque> */
/* #include <string> */
/* #include <sstream> */
/* #include <cassert> */

/* #include "../Utils.h" */

/* #define MININT -INT_MIN/2 //prevent underflow */

/* class AlignmentParams { */
/* public: */
/*   int matchScore = 2; */
/*   int mismatchScore = -4; */

/*   int terminalGapOpenPenalty = 2; */
/*   int terminalGapExtensionPenalty = 1; */

/*   int interiorGapOpenPenalty = 20; */
/*   int interiorGapExtensionPenalty = 2; */

/*   int GapOpenScore( bool terminal ) const { */
/*     return -( terminal ? terminalGapOpenPenalty : interiorGapOpenPenalty ); */
/*   } */

/*   int GapExtensionScore( int length, bool terminal ) const { */
/*     return length * -( terminal ? terminalGapExtensionPenalty : interiorGapExtensionPenalty ); */
/*   } */

/*   int GapScore( int length, bool terminal ) const { */
/*     return GapOpenScore( terminal ) + GapExtensionScore( length, terminal ); */
/*   } */
/* }; */

/* typedef enum { */
/*   CIGAR_UNKNOWN   = 0, */
/*   CIGAR_MATCH     = 'M', */
/*   CIGAR_DELETION  = 'D', */
/*   CIGAR_INSERTION = 'I', */
/* } CigarOp; */

/* class CigarEntry { */
/* public: */
/*   int length; */
/*   CigarOp op; */

/*   CigarEntry() */
/*     : CigarEntry( 0, CIGAR_UNKNOWN ) */
/*   { */
/*   } */

/*   CigarEntry( int length, CigarOp op ) */
/*     : length( length ), op( op ) { } */
/* }; */
/* typedef std::deque< CigarEntry > Cigar; */

/* class Alignment { */
/* public: */
/*   Alignment() */
/*     : mScore( 0 ) */
/*   { */
/*   } */

/*   Alignment( const Sequence &A, const Sequence &B, int score, const Cigar &cigar = ::Cigar()  ) */
/*     : mSequenceA( A ), mSequenceB( B ), mScore( score ), mCigar( cigar ) */
/*   { */
/*   } */

/*   int Score() const { */
/*     return mScore; */
/*   } */

/*   ::Cigar Cigar() const { */
/*     return mCigar; */
/*   } */

/*   friend std::ostream &operator<<( std::ostream &os, const Alignment &aln ) { */

/*     std::string q; */
/*     std::string t; */
/*     std::string a; */

/*     const Sequence &query = aln.mSequenceA; */
/*     const Sequence &target = aln.mSequenceB; */

/*     size_t queryStart = 0; */
/*     size_t targetStart = 0; */
/*     size_t queryEnd = query.Length(); */
/*     size_t targetEnd = target.Length(); */

/*     ::Cigar cigar = aln.Cigar(); */

/*     // Dont take left terminal gap into account */
/*     if( !cigar.empty() ) { */
/*       const CigarEntry &fce = cigar.front(); */
/*       if( fce.op != CIGAR_MATCH ) { */
/*         if( fce.op == CIGAR_DELETION ) { */
/*           targetStart = fce.length; */
/*         } else { */
/*           queryStart = fce.length; */
/*         } */
/*         cigar.pop_front(); */
/*       } */
/*     } */

/*     // Don't take right terminal gap into account */
/*     if( !cigar.empty() ) { */
/*       const CigarEntry &bce = cigar.back(); */
/*       if( bce.op != CIGAR_MATCH ) { */
/*         if( bce.op == CIGAR_DELETION ) { */
/*           targetEnd = target.Length() - bce.length; */
/*         } else { */
/*           queryEnd = query.Length() - bce.length; */
/*         } */
/*         cigar.pop_back(); */
/*       } */
/*     } */

/*     bool match; */
/*     size_t numMatches = 0; */
/*     size_t numCols = 0; */

/*     size_t qcount = queryStart; */
/*     size_t tcount = targetStart; */

/*     for( auto &c : cigar ) { */
/*       for( int i = 0; i < c.length; i++ ) { */
/*         switch( c.op ) { */
/*           case CIGAR_INSERTION: */
/*             t += '-'; */
/*             q += query[ qcount++ ]; */
/*             a += ' '; */
/*             break; */

/*           case CIGAR_DELETION: */
/*             q += '-'; */
/*             t += target[ tcount++ ]; */
/*             a += ' '; */
/*             break; */

/*           case CIGAR_MATCH: */
/*             match = DoNucleotidesMatch( query[ qcount ], target[ tcount ] ); */
/*             if( match ) { */
/*               numMatches++; */
/*             } */
/*             a += match ? '|' : ' '; */
/*             q += query[ qcount++ ]; */
/*             t += target[ tcount++ ]; */
/*             break; */

/*           default: */
/*             break; */
/*         } */

/*         numCols++; */
/*       } */
/*     } */

/*     os << std::endl; */
/*     os << "QRY " << std::setw( 10 ) << ( queryStart + 1 ) << " " << q << " " << ( queryEnd ) << std::endl; */
/*     os << std::string( 15, ' ' ) << a << std::endl; */
/*     os << "REF " << std::setw( 10 ) << ( targetStart + 1 ) << " " << t << " " << ( targetEnd ) << std::endl; */
/*     os << std::endl; */
/*     os <<  numCols << " cols, " << numMatches << " ids (" << */
/*       std::fixed << std::setprecision( 1 ) << ( 100.0f * float( numMatches ) / float( numCols ) ) */
/*       << "%)" << std::endl; */
/*     return os; */
/*   } */

/* private: */
/*   Sequence mSequenceA, mSequenceB; */
/*   int mScore; */
/*   ::Cigar mCigar; */
/* }; */

/* class DPAlign { */
/* public: */
/*   DPAlign( AlignmentParams ap = AlignmentParams() ) */
/*     : mAP( ap ), mWidth( 0 ), mHeight( 0 ), mCells( NULL ), mCellInitialized( NULL ), mNumCellsReserved( 0 ) */
/*   { */
/*   } */

/*   int Align( const Sequence &A, const Sequence &B, Alignment *aln = NULL ) { */
/*     mSequenceA = A; */
/*     mSequenceB = B; */

/*     mWidth = A.Length() + 1; */
/*     mHeight = B.Length() + 1; */

/*     // Enlarge cells if we need more */
/*     // (Reserving memory is a heavy task) */
/*     if( mNumCellsReserved < mWidth * mHeight ) { */
/*       std::cout << "Resize cells " << mNumCellsReserved << " -> " << (mWidth * mHeight) << std::endl; */

/*       if( mCells ) */
/*         delete[] mCells; */

/*       if( mCellInitialized ) */
/*         delete[] mCellInitialized; */

/*       mNumCellsReserved = mWidth * mHeight; */
/*       mCells = new Cell[ mNumCellsReserved ]; */
/*       mCellInitialized = new bool[ mNumCellsReserved ]; */
/*     } */

/*     // Reset matrix */
/*     memset( mCellInitialized, false, mNumCellsReserved * sizeof( bool ) ); */

/*     // Now Compute Matrix */
/*     ComputeMatrix(); */

/*     // Populate aln */
/*     int score = Score(); */

/*     if( aln ) { */
/*       *aln = Alignment( A, B, score, Cigar() ); */
/*     } */

/*     return score; */
/*   } */

/*   virtual ~DPAlign() { */
/*     if( mCells ) { */
/*       delete[] mCells; */
/*     } */

/*     if( mCellInitialized ) { */
/*       delete[] mCellInitialized; */
/*     } */
/*   } */

/*   void DebugPrint( bool withArrows = false ) { */
/*     for( size_t x = 0; x < mWidth; x++ ) { */
/*       if( x == 0 ) { */
/*         printf( "      " ); */
/*       } else { */
/*         printf( "     %c", mSequenceA[ x - 1 ] ); */
/*       } */
/*     } */
/*     printf( "\n"); */

/*     for( size_t y = 0; y < mHeight; y++ ) { */
/*       for( size_t x = 0; x < mWidth; x++ ) { */
/*         if( x == 0 ) { */
/*           char ch = ' '; */
/*           if( y > 0 ) { */
/*             ch = mSequenceB[ y - 1 ]; */
/*           } */
/*           printf( "%c", ch ); */
/*         } */

/*         if( cell( x, y ).score > MININT ) { */
/*           printf( " %4d", cell( x, y ).score ); */

/*           if( withArrows ) { */
/*             if( cell( x, y ).score == cell( x, y ).hGap ) { */
/*               printf("←"); */
/*             } else if( cell( x, y ).score == cell( x, y ).vGap ) { */
/*               printf("↑"); */
/*             } else { */
/*               printf("↖"); */
/*             } */
/*           } else  { */
/*             printf(" "); */
/*           } */
/*         } else { */
/*           printf( "    X " ); */
/*         } */
/*       } */

/*       printf( "\n" ); */
/*     } */
/*   } */

/*   virtual void ComputeMatrix() = 0; */

/*   int Score() { */
/*     size_t x, y; */
/*     if( !TracebackStartingPosition( x, y ) ) */
/*       return MININT; */

/*     return cell( x, y ).score; */
/*   } */

/*   Cigar Cigar() { */
/*     size_t x, y; */
/*     if( !TracebackStartingPosition( x, y ) ) */
/*       return Cigar(); */

/*     std::deque< CigarOp > rawOps; */
/*     while( true ) { */
/*       const Cell &cur = cell( x, y ); */
/*       if( cur.score <= MININT ) */
/*         break; */

/*       if( cur.score == cur.hGap ) { */
/*         // go left */
/*         if( x == 0 ) */
/*           break; */

/*         x--; */
/*         rawOps.push_back( CIGAR_INSERTION ); */
/*       } else if( cur.score == cur.vGap ) { */
/*         // go up */
/*         if( y == 0 ) */
/*           break; */

/*         y--; */
/*         rawOps.push_back( CIGAR_DELETION ); */
/*       } else { */
/*         // go diagonal */
/*         if( x == 0 || y == 0 ) */
/*           break; */

/*         x--; */
/*         y--; */
/*         rawOps.push_back( CIGAR_MATCH ); */
/*       } */
/*     } */

/*     ::Cigar cigar; */
/*     CigarOp lastOp = CIGAR_UNKNOWN; */
/*     size_t lastOpCount = 0; */
/*     for( auto it = rawOps.rbegin(); it != rawOps.rend(); it++ ) { */
/*       if( *it != lastOp ) { */
/*         if( lastOp != CIGAR_UNKNOWN ) { */
/*           cigar.push_back( CigarEntry( lastOpCount, lastOp ) ); */
/*         } */
/*         lastOp = *it; */
/*         lastOpCount = 0; */
/*       } */
/*       lastOpCount++; */
/*     } */
/*     if( lastOp != CIGAR_UNKNOWN ) { */
/*       cigar.push_back( CigarEntry( lastOpCount, lastOp ) ); */
/*     } */

/*     return cigar; */
/*   } */

/* protected: */
/*   virtual bool TracebackStartingPosition( size_t &x, size_t &y ) const = 0; */

/*   struct Cell { */
/*     int score, vGap, hGap; */

/*     Cell() { Reset(); } */
/*     Cell( int score, int vGap, int hGap ) : score( score ), vGap( vGap ), hGap( hGap ) { } */

/*     void Reset() { */
/*       score = vGap = hGap = MININT; */
/*     } */
/*   }; */

/*   Cell& ComputeCell( size_t x, size_t y ) { */
/*     Cell& cur = cell( x, y ); */

/*     if( x == 0 && y == 0 ) { */
/*       cur.score = cur.vGap = cur.hGap = 0; */
/*       return cur; */
/*     } */

/*     if( y == 0 ) { */
/*       cur.score = cur.hGap = mAP.GapScore( x, true ); */
/*       return cur; */
/*     } */

/*     if( x == 0 ) { */
/*       cur.score = cur.vGap = mAP.GapScore( y, true ); */
/*       return cur; */
/*     } */

/*     const Cell& leftCell = cell( x - 1, y ); */
/*     const Cell& upperCell = cell( x, y - 1 ); */
/*     const Cell& leftUpperCell = cell( x - 1, y - 1 ); */

/*     bool terminalRow = ( y == 0 || y == mHeight - 1 ); */
/*     bool terminalCol = ( x == 0 || x == mWidth - 1 ); */

/*     cur.hGap = std::max( */
/*         leftCell.score + mAP.GapScore( 1, terminalRow ), */
/*         leftCell.hGap + mAP.GapExtensionScore( 1, terminalRow ) ); */

/*     cur.vGap = std::max( */
/*         upperCell.score + mAP.GapScore( 1, terminalCol ), */
/*         upperCell.vGap + mAP.GapExtensionScore( 1, terminalCol ) ); */

/*     int diagScore = leftUpperCell.score + */
/*       ( DoNucleotidesMatch( mSequenceA[ x - 1 ], mSequenceB[ y - 1 ] ) ? mAP.matchScore : mAP.mismatchScore ); */
/*     cur.score = std::max( { diagScore, cur.hGap, cur.vGap } ); */
/*     return cur; */
/*   } */

/*   inline Cell& cell( size_t x, size_t y ) { */
/*     assert( y >= 0 && y < mHeight ); */
/*     assert( x >= 0 && x < mWidth ); */

/*     size_t idx = y * mWidth + x; */

/*     Cell &cur = mCells[ idx ]; */
/*     bool &init = mCellInitialized[ idx ]; */

/*     if( !init ) { */
/*       cur.Reset(); */
/*       init = true; */
/*     } */

/*     return cur; */
/*   } */

/*   size_t mWidth, mHeight; */
/*   Sequence mSequenceA, mSequenceB; */
/*   Cell *mCells; */
/*   bool *mCellInitialized; */
/*   size_t mNumCellsReserved; */
/*   AlignmentParams mAP; */
/* }; */

/* class XDropExtension : public DPAlign */
/* { */
/* public: */
/*   XDropExtension( int xDrop, AlignmentParams ap = AlignmentParams() ) */
/*     : DPAlign( ap ), mXDrop( xDrop ), mBestX( 0 ), mBestY( 0 ) */
/*   { */
/*   } */

/*   void ComputeMatrix() { */
/*     int bestScore = MININT; */

/*     size_t leftBound = 0, rightBound = 0; */

/*     for( size_t x = 0; x < mWidth; x++ ) { */
/*       Cell &cur = ComputeCell( x, 0 ); */
/*       if( cur.score < -mXDrop ) { */
/*         cur.Reset(); */
/*         break; */
/*       } */

/*       rightBound = x; */
/*     } */

/*     for( size_t y = 1; y < mHeight; y++ ) { */
/*       size_t nextLeftBound = leftBound; */
/*       size_t nextRightBound = leftBound; */

/*       for( size_t x = leftBound; x <= rightBound; x++ ) { */
/*         Cell &cur = ComputeCell( x, y ); */

/*         if( bestScore - cur.score > mXDrop ) { */
/*           if( x == nextLeftBound && nextLeftBound + 1 < mWidth ) { */
/*             nextLeftBound++; */
/*           } */
/*         } else { */
/*           nextRightBound = x; */

/*           bestScore = std::max( cur.score, bestScore ); */
/*           if( bestScore == cur.score ) { */
/*             mBestX = x; */
/*             mBestY = y; */
/*           } */
/*         } */
/*       } */

/*       if( nextRightBound == rightBound ) { */
/*         // This row may not be finished yet, so extend it until x-drop test fails */
/*         size_t cursor = nextRightBound; */
/*         while( cursor < mWidth ) { */
/*           Cell& cur = ComputeCell( cursor, y ); */
/*           if( bestScore - cur.score > mXDrop ) { */
/*             break; */
/*           } */
/*           nextRightBound = cursor; */
/*           cursor++; */
/*         } */
/*       } */

/*       leftBound = nextLeftBound; */
/*       rightBound = nextRightBound; */
/*     } */
/*   } */

/*   bool TracebackStartingPosition( size_t &tx, size_t &ty ) const { */
/*     tx = mBestX; */
/*     ty = mBestY; */
/*     return true; */
/*   } */

/* private: */
/*   int mXDrop; */
/*   size_t mBestX, mBestY; */
/* }; */

/* class GuidedBandedGlobalAlign : public DPAlign */
/* { */
/*   class GuidePoint { */
/*   public: */
/*     size_t x, y; */

/*     bool operator<( const GuidePoint &other ) const { */
/*       if( y < other.y ) */
/*         return true; */
/*       if( y > other.y ) */
/*         return false; */
/*       return x < other.x; */
/*     } */

/*     GuidePoint( size_t x = 0, size_t y = 0 ) */
/*       : x( x ), y ( y ) */
/*     { */
/*     } */
/*   }; */

/* public: */
/*   GuidedBandedGlobalAlign( size_t bandWidth, AlignmentParams ap = AlignmentParams() ) */
/*     : DPAlign( ap ), mBandWidth( bandWidth ) */
/*   { */
/*   } */

/*   int AlignAlongChain( const Sequence &A, const Sequence &B, const SeedList &chain = SeedList(), Alignment *aln = NULL ) { */
/*     mGuidePoints.clear(); */

/*     for( auto &pos : chain ) { */
/*       mGuidePoints.insert( GuidePoint( pos.s1, pos.s2 ) ); */
/*       mGuidePoints.insert( GuidePoint( pos.s1 + pos.length, pos.s2 + pos.length ) ); */
/*     } */

/*     if( !mGuidePoints.empty() ) { */
/*       // Start position must intersect with matrix */
/*       auto &fgp = *mGuidePoints.begin(); */
/*       if( fgp.x != 0 && fgp.y != 0 ) { */
/*         size_t offset = std::min( fgp.x, fgp.y ); */
/*         mGuidePoints.insert( GuidePoint( fgp.x - offset, fgp.y - offset ) ); */
/*       } */

/*       // End position must intersect with matrix */
/*       auto &lgp = *mGuidePoints.rbegin(); */
/*       if( lgp.x != A.Length() && lgp.y != B.Length() ) { */
/*         size_t offset = std::min( A.Length() - lgp.x, B.Length() - lgp.y ); */
/*         mGuidePoints.insert( GuidePoint( lgp.x + offset, lgp.y + offset ) ); */
/*       } */
/*     } else { */
/*       // Straight diagonal */
/*       mGuidePoints.insert( GuidePoint( 0, 0 ) ); */
/*       mGuidePoints.insert( GuidePoint( A.Length(), B.Length() ) ); */
/*     } */

/*     return Align( A, B, aln ); */
/*   } */

/*   void ComputeMatrix() { */
/*     if( mGuidePoints.empty() ) */
/*       return; */

/*     /1* for( auto &gp : mGuidePoints ) { *1/ */
/*     /1*   std::cout << "GuidePoint x: " << gp.x << " y: " << gp.y << std::endl; *1/ */
/*     /1* } *1/ */

/*     const GuidePoint& firstGP = *mGuidePoints.begin(); */
/*     const GuidePoint& lastGP = *mGuidePoints.rbegin(); */

/*     size_t lastCursor = 0; */

/*     // Calculate from 0, 0 to intersection point (first GP) */
/*     if( firstGP.x == 0 ) { */
/*       // Intersects at Y-axis */
/*       for( size_t y = 0; y < firstGP.y; y++ ) { */
/*         ComputeCell( 0, y ); */
/*       } */
/*     } else { */
/*       // Intersects at X-axis */
/*       for( size_t x = 0; x < firstGP.x; x++ ) { */
/*         ComputeCell( x, 0 ); */
/*       } */
/*     } */

/*     for( size_t y = firstGP.y; y <= lastGP.y; y++ ) { */
/*       // Current guide point */
/*       auto currentIt = mGuidePoints.upper_bound( GuidePoint( 0, y ) ); */
/*       if( currentIt != mGuidePoints.begin() ) { */
/*         currentIt--; */
/*       } */
/*       const GuidePoint& currentGP = *currentIt; */

/*       // Next guide point */
/*       auto nextIt = mGuidePoints.upper_bound( currentGP ); */
/*       assert( nextIt != mGuidePoints.end() ); */
/*       const GuidePoint& nextGP = *nextIt; */

/*       float ratio = float( y - currentGP.y ) / float( nextGP.y - currentGP.y ); */
/*       size_t cursor = currentGP.x + ratio * ( nextGP.x - currentGP.x); */

/*       size_t xFirst = ( cursor > mBandWidth ) ? ( cursor - mBandWidth ) : 0; */
/*       size_t xLast = ( cursor + mBandWidth < mWidth - 1 ) ? ( cursor + mBandWidth ) : ( mWidth - 1 ); */

/*       // Make sure we can connect to the previous row */
/*       // (in case we jump far) */
/*       if( xFirst > lastCursor ) */
/*         xFirst = lastCursor; */
/*       lastCursor = cursor; */

/*       /1* std::cout << "Y " << y *1/ */
/*       /1*   << " CurrentGP " << "(" << currentGP.x << "," << currentGP.y << ")" *1/ */
/*       /1*   << " NextGP " << "(" << nextGP.x << "," << nextGP.y << ")" *1/ */
/*       /1*   << std::endl; *1/ */
/*       /1* std::cout << "Ratio " << ratio << std::endl; *1/ */
/*       /1* std::cout << "Cursor " << cursor << std::endl; *1/ */
/*       for( size_t x = xFirst; x <= xLast; x++ ) { */
/*         ComputeCell( x, y ); */
/*       } */
/*     } // for y */

/*     // Calculate from last GP to end of matrix */
/*     if( lastGP.x == mWidth - 1 ) { */
/*       // Intersects at Y-axis */
/*       for( size_t y = lastGP.y; y < mHeight; y++ ) { */
/*         ComputeCell( mWidth - 1, y ); */
/*       } */
/*     } else { */
/*       // Intersects at X-axis */
/*       for( size_t x = lastGP.x; x < mWidth; x++ ) { */
/*         ComputeCell( x, mHeight - 1 ); */
/*       } */
/*     } */
/*   } */

/*   bool TracebackStartingPosition( size_t &tx, size_t &ty ) const { */
/*     // Needleman-Wunsch */
/*     tx = mWidth - 1; */
/*     ty = mHeight - 1; */

/*     return true; */
/*   } */

/* private: */
/*   size_t mBandWidth; */
/*   std::multiset< GuidePoint > mGuidePoints; */
/* }; */
