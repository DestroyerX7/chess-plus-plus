#include <emscripten.h>
#include <cstdint>
#include <string>
#include <limits>

using Bitboard = uint64_t;
using Move = uint16_t;
using Piece = int8_t;

struct Position
{
    Bitboard piece_bitboards[2][6];
    Bitboard occupancy_bitboards[3];
    uint8_t color_to_move = 0;

    Piece pieces[64];

    uint8_t castling_rights = 0xf;
    uint8_t has_castled = 0;
};

struct ChessboardRing
{
    Bitboard bitboard;
    double value;
};

struct Unmove
{
    Move move = 0;
    Piece captured_piece = 0;
    uint8_t castling_rights = 0;
};

static Position position;

constexpr int kPieceValues[6] = {1, 3, 3, 5, 9, 1024};

constexpr ChessboardRing kChessboardRings[4] = {
    {0xff818181818181ffULL, 0},
    {0x007e424242427e00ULL, 1.0 / 3},
    {0x00003c24243c0000ULL, 2.0 / 3},
    {0x0000001818000000ULL, 1}};

constexpr Bitboard kAFileBitboard = 0x0101010101010101ULL;
constexpr Bitboard kBFileBitboard = 0x0202020202020202ULL;
constexpr Bitboard kGFileBitboard = 0x4040404040404040ULL;
constexpr Bitboard kHFileBitboard = 0x8080808080808080ULL;

Bitboard knight_attacks_bitboards[64];
Bitboard king_attacks_bitboards[64];

// Move types
// 0 - Basic
// 1 - Capture
// 2 - Castle
// 3 - Promotion
// 4 - En Passant
// 5 - Check

void setup_knight_attacks_bitboards(Bitboard knight_attacks_bitboards[])
{
    for (int i = 0; i < 64; i++)
    {
        Bitboard from_square_bitboard = 1ULL << i;
        Bitboard attacks_bitboard = 0;

        attacks_bitboard |= (from_square_bitboard << 6) & (~kGFileBitboard) & (~kHFileBitboard);
        attacks_bitboard |= (from_square_bitboard << 10) & (~kAFileBitboard) & (~kBFileBitboard);
        attacks_bitboard |= (from_square_bitboard << 15) & (~kHFileBitboard);
        attacks_bitboard |= (from_square_bitboard << 17) & (~kAFileBitboard);

        attacks_bitboard |= (from_square_bitboard >> 6) & (~kAFileBitboard) & (~kBFileBitboard);
        attacks_bitboard |= (from_square_bitboard >> 10) & (~kGFileBitboard) & (~kHFileBitboard);
        attacks_bitboard |= (from_square_bitboard >> 15) & (~kAFileBitboard);
        attacks_bitboard |= (from_square_bitboard >> 17) & (~kHFileBitboard);

        knight_attacks_bitboards[i] = attacks_bitboard;
    }
}

void setup_king_attacks_bitboards(Bitboard king_attacks_bitboards[])
{
    for (int i = 0; i < 64; i++)
    {
        Bitboard from_square_bitboard = 1ULL << i;
        Bitboard attacks_bitboard = 0;

        attacks_bitboard |= (from_square_bitboard << 1) & (~kAFileBitboard);
        attacks_bitboard |= (from_square_bitboard << 7) & (~kHFileBitboard);
        attacks_bitboard |= (from_square_bitboard << 8);
        attacks_bitboard |= (from_square_bitboard << 9) & (~kAFileBitboard);

        attacks_bitboard |= (from_square_bitboard >> 1) & (~kHFileBitboard);
        attacks_bitboard |= (from_square_bitboard >> 7) & (~kAFileBitboard);
        attacks_bitboard |= (from_square_bitboard >> 8);
        attacks_bitboard |= (from_square_bitboard >> 9) & (~kHFileBitboard);

        king_attacks_bitboards[i] = attacks_bitboard;
    }
}

double evaluate_position(const Position &position)
{
    double eval = 0;

    for (int i = 0; i < 6; i++)
    {
        int white_peice_count = __builtin_popcountll(position.piece_bitboards[0][i]);
        eval += white_peice_count * kPieceValues[i];

        int black_piece_count = __builtin_popcountll(position.piece_bitboards[1][i]);
        eval -= black_piece_count * kPieceValues[i];
    }

    for (const ChessboardRing &chessboard_ring : kChessboardRings)
    {
        int white_chessboard_ring_peice_count = __builtin_popcountll(position.occupancy_bitboards[0] & chessboard_ring.bitboard);
        eval += white_chessboard_ring_peice_count * chessboard_ring.value;

        int black_chessboard_ring_piece_count = __builtin_popcountll(position.occupancy_bitboards[1] & chessboard_ring.bitboard);
        eval -= black_chessboard_ring_piece_count * chessboard_ring.value;
    }

    if ((position.has_castled & 1) != 0)
    {
        eval += 0.5;
    }

    if ((position.has_castled & 2) != 0)
    {
        eval -= 0.5;
    }

    return eval;
}

int piece_to_type(const Piece &piece)
{
    if (piece == 0)
    {
        return -1;
    }

    int type = abs(piece) - 1;
    return type;
}

void make_move(const Move &move, Position &position)
{
    int flag = (move >> 12) & 0xf;
    int from_square = (move >> 6) & 0x3f;
    int to_square = move & 0x3f;
    Bitboard from_square_bitboard = 1ULL << from_square;
    Bitboard to_square_bitboard = 1ULL << to_square;

    int move_piece_type = piece_to_type(position.pieces[from_square]);

    if (position.pieces[from_square] == 6)
    {
        position.castling_rights &= 0xc;
    }
    else if (position.pieces[from_square] == -6)
    {
        position.castling_rights &= 0x3;
    }
    else if (move_piece_type == 3 && from_square == 7)
    {
        position.castling_rights &= 0xe;
    }
    else if (move_piece_type == 3 && from_square == 0)
    {
        position.castling_rights &= 0xd;
    }
    else if (move_piece_type == 3 && from_square == 63)
    {
        position.castling_rights &= 0xb;
    }
    else if (move_piece_type == 3 && from_square == 56)
    {
        position.castling_rights &= 0x7;
    }

    int move_color = position.color_to_move;
    int opposite_color = position.color_to_move == 0 ? 1 : 0;

    if (position.pieces[to_square] != 0)
    {
        int captured_piece_type = piece_to_type(position.pieces[to_square]);
        position.piece_bitboards[opposite_color][captured_piece_type] &= ~to_square_bitboard;
        position.occupancy_bitboards[opposite_color] &= ~to_square_bitboard;
    }

    position.piece_bitboards[move_color][move_piece_type] &= ~from_square_bitboard;

    if (flag == 0)
    {
        position.piece_bitboards[move_color][move_piece_type] |= to_square_bitboard;

        position.pieces[to_square] = position.pieces[from_square];
    }
    else if (flag == 2)
    {
        position.piece_bitboards[move_color][move_piece_type] |= to_square_bitboard;

        position.pieces[to_square] = position.pieces[from_square];

        if (to_square == 6)
        {
            position.piece_bitboards[0][3] &= ~(1ULL << 7);
            position.piece_bitboards[0][3] |= 1ULL << 5;

            position.pieces[7] = 0;
            position.pieces[5] = 4;

            position.occupancy_bitboards[0] &= ~(1ULL << 7);
            position.occupancy_bitboards[0] |= 1ULL << 5;

            position.has_castled |= 1;
        }
        else if (to_square == 2)
        {
            position.piece_bitboards[0][3] &= ~1ULL;
            position.piece_bitboards[0][3] |= 1ULL << 3;

            position.pieces[0] = 0;
            position.pieces[3] = 4;

            position.occupancy_bitboards[0] &= ~1ULL;
            position.occupancy_bitboards[0] |= 1ULL << 3;

            position.has_castled |= 1;
        }
        else if (to_square == 62)
        {
            position.piece_bitboards[1][3] &= ~(1ULL << 63);
            position.piece_bitboards[1][3] |= 1ULL << 61;

            position.pieces[63] = 0;
            position.pieces[61] = -4;

            position.occupancy_bitboards[1] &= ~(1ULL << 63);
            position.occupancy_bitboards[1] |= 1ULL << 61;

            position.has_castled |= 2;
        }
        else if (to_square == 58)
        {
            position.piece_bitboards[1][3] &= ~(1ULL << 56);
            position.piece_bitboards[1][3] |= 1ULL << 59;

            position.pieces[56] = 0;
            position.pieces[59] = -4;

            position.occupancy_bitboards[1] &= ~(1ULL << 56);
            position.occupancy_bitboards[1] |= 1ULL << 59;

            position.has_castled |= 2;
        }
    }
    else if (flag == 3)
    {
        position.piece_bitboards[move_color][4] |= to_square_bitboard;

        position.pieces[to_square] = move_color == 0 ? 5 : -5;
    }

    position.pieces[from_square] = 0;

    position.occupancy_bitboards[move_color] &= ~from_square_bitboard;
    position.occupancy_bitboards[move_color] |= to_square_bitboard;
    position.occupancy_bitboards[2] = position.occupancy_bitboards[0] | position.occupancy_bitboards[1];

    position.color_to_move = opposite_color;
}

void unmake_move(const Unmove &unmove, Position &position)
{
    int flag = (unmove.move >> 12) & 0xf;
    int from_square = (unmove.move >> 6) & 0x3f;
    int to_square = unmove.move & 0x3f;
    Bitboard from_square_bitboard = 1ULL << from_square;
    Bitboard to_square_bitboard = 1ULL << to_square;

    int move_color = position.color_to_move == 0 ? 1 : 0;
    int opposite_color = position.color_to_move;

    if (flag == 0)
    {
        int move_piece_type = piece_to_type(position.pieces[to_square]);

        position.piece_bitboards[move_color][move_piece_type] &= ~to_square_bitboard;
        position.piece_bitboards[move_color][move_piece_type] |= from_square_bitboard;

        position.pieces[from_square] = position.pieces[to_square];
        position.pieces[to_square] = unmove.captured_piece;
    }
    else if (flag == 2)
    {
        int move_piece_type = piece_to_type(position.pieces[to_square]);

        position.piece_bitboards[move_color][move_piece_type] &= ~to_square_bitboard;
        position.piece_bitboards[move_color][move_piece_type] |= from_square_bitboard;

        position.pieces[from_square] = position.pieces[to_square];
        position.pieces[to_square] = unmove.captured_piece;

        if (to_square == 6)
        {
            position.piece_bitboards[0][3] |= 1ULL << 7;
            position.piece_bitboards[0][3] &= ~(1ULL << 5);

            position.pieces[7] = 4;
            position.pieces[5] = 0;

            position.occupancy_bitboards[0] |= 1ULL << 7;
            position.occupancy_bitboards[0] &= ~(1ULL << 5);

            position.has_castled &= ~1;
        }
        else if (to_square == 2)
        {
            position.piece_bitboards[0][3] |= 1ULL;
            position.piece_bitboards[0][3] &= ~(1ULL << 3);

            position.pieces[0] = 4;
            position.pieces[3] = 0;

            position.occupancy_bitboards[0] |= 1ULL;
            position.occupancy_bitboards[0] &= ~(1ULL << 3);

            position.has_castled &= ~1;
        }
        else if (to_square == 62)
        {
            position.piece_bitboards[1][3] |= 1ULL << 63;
            position.piece_bitboards[1][3] &= ~(1ULL << 61);

            position.pieces[63] = -4;
            position.pieces[61] = 0;

            position.occupancy_bitboards[1] |= 1ULL << 63;
            position.occupancy_bitboards[1] &= ~(1ULL << 61);

            position.has_castled &= ~2;
        }
        else if (to_square == 58)
        {
            position.piece_bitboards[1][3] |= 1ULL << 56;
            position.piece_bitboards[1][3] &= ~(1ULL << 59);

            position.pieces[56] = -4;
            position.pieces[59] = 0;

            position.occupancy_bitboards[1] |= 1ULL << 56;
            position.occupancy_bitboards[1] &= ~(1ULL << 59);

            position.has_castled &= ~2;
        }
    }
    else if (flag == 3)
    {
        int move_piece_type = 0;

        position.piece_bitboards[move_color][4] &= ~to_square_bitboard;
        position.piece_bitboards[move_color][move_piece_type] |= from_square_bitboard;

        position.pieces[from_square] = move_color == 0 ? 1 : -1;
        position.pieces[to_square] = unmove.captured_piece;
    }

    position.occupancy_bitboards[move_color] &= ~to_square_bitboard;
    position.occupancy_bitboards[move_color] |= from_square_bitboard;

    if (unmove.captured_piece != 0)
    {
        int captured_piece_type = piece_to_type(unmove.captured_piece);
        position.piece_bitboards[opposite_color][captured_piece_type] |= to_square_bitboard;
        position.occupancy_bitboards[opposite_color] |= to_square_bitboard;
    }

    position.occupancy_bitboards[2] = position.occupancy_bitboards[0] | position.occupancy_bitboards[1];

    position.castling_rights = unmove.castling_rights;

    position.color_to_move = move_color;
}

Bitboard get_pawn_moves_bitboard(int from_square, const Position &position)
{
    Bitboard from_square_bitbaord = 1ULL << from_square;
    Bitboard moves_bitbaord = 0;

    if (position.color_to_move == 0)
    {
        Bitboard single_push_bitboard = (from_square_bitbaord << 8) & ~position.occupancy_bitboards[2];
        moves_bitbaord |= single_push_bitboard;

        moves_bitbaord |= (from_square_bitbaord << 7) & (~kHFileBitboard) & position.occupancy_bitboards[1];
        moves_bitbaord |= (from_square_bitbaord << 9) & (~kAFileBitboard) & position.occupancy_bitboards[1];

        if (from_square >= 8 && from_square <= 15 && single_push_bitboard != 0)
        {
            moves_bitbaord |= (from_square_bitbaord << 16) & ~position.occupancy_bitboards[2];
        }
    }
    else
    {
        Bitboard single_push_bitboard = (from_square_bitbaord >> 8) & ~position.occupancy_bitboards[2];
        moves_bitbaord |= single_push_bitboard;

        moves_bitbaord |= (from_square_bitbaord >> 7) & (~kAFileBitboard) & position.occupancy_bitboards[0];
        moves_bitbaord |= (from_square_bitbaord >> 9) & (~kHFileBitboard) & position.occupancy_bitboards[0];

        if (from_square >= 48 && from_square <= 55 && single_push_bitboard != 0)
        {
            moves_bitbaord |= (from_square_bitbaord >> 16) & ~position.occupancy_bitboards[2];
        }
    }

    return moves_bitbaord;
}

Bitboard get_knight_moves_bitboard(int from_square, const Position &position)
{
    int opposite_color = position.color_to_move == 0 ? 1 : 0;
    return knight_attacks_bitboards[from_square] & (position.occupancy_bitboards[opposite_color] | (~position.occupancy_bitboards[2]));
}

Bitboard get_bishop_moves_bitboard(int from_square, const Position &position)
{
    int opposite_color = position.color_to_move == 0 ? 1 : 0;

    Bitboard from_square_bitboard = 1ULL << from_square;
    Bitboard moves_bitboard = 0;

    int current = 7;

    while ((from_square_bitboard & kAFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard << current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kAFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current += 7;
    }

    current = 7;

    while ((from_square_bitboard & kHFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard >> current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kHFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current += 7;
    }

    current = 9;

    while ((from_square_bitboard & kHFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard << current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kHFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current += 9;
    }

    current = 9;

    while ((from_square_bitboard & kAFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard >> current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kAFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current += 9;
    }

    return moves_bitboard;
}

Bitboard get_rook_moves_bitboard(int from_square, const Position &position)
{
    int opposite_color = position.color_to_move == 0 ? 1 : 0;

    Bitboard from_square_bitboard = 1ULL << from_square;
    Bitboard moves_bitboard = 0;

    int current = 8;

    while (1)
    {
        Bitboard yo = from_square_bitboard << current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || yo == 0)
        {
            break;
        }

        current += 8;
    }

    current = 8;

    while (1)
    {
        Bitboard yo = from_square_bitboard >> current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || yo == 0)
        {
            break;
        }

        current += 8;
    }

    current = 1;

    while ((from_square_bitboard & kHFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard << current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kHFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current++;
    }

    current = 1;

    while ((from_square_bitboard & kAFileBitboard) == 0)
    {
        Bitboard yo = from_square_bitboard >> current;
        moves_bitboard |= yo & (~position.occupancy_bitboards[position.color_to_move]);

        if ((yo & position.occupancy_bitboards[2]) != 0 || (yo & kAFileBitboard) != 0 || yo == 0)
        {
            break;
        }

        current++;
    }

    return moves_bitboard;
}

Bitboard get_queen_moves_bitboard(int from_square, const Position &position)
{
    return get_bishop_moves_bitboard(from_square, position) | get_rook_moves_bitboard(from_square, position);
}

Bitboard get_king_moves_bitboard(int from_square, const Position &position)
{
    int opposite_color = position.color_to_move == 0 ? 1 : 0;
    return king_attacks_bitboards[from_square] & (position.occupancy_bitboards[opposite_color] | (~position.occupancy_bitboards[2]));
}

void generate_all_moves(Move moves[], int &length, const Position &position)
{
    int move_color = position.color_to_move;
    int opposite_color = position.color_to_move == 0 ? 1 : 0;
    Bitboard pawns_bitboard = position.piece_bitboards[move_color][0];

    while (pawns_bitboard != 0)
    {
        int from_square = __builtin_ctzll(pawns_bitboard);
        Bitboard pawn_moves_bitbaord = get_pawn_moves_bitboard(from_square, position);

        while (pawn_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(pawn_moves_bitbaord);
            Move move = (from_square << 6) | to_square;

            if (move_color == 0 && to_square >= 56 && to_square <= 63)
            {
                move |= 3 << 12;
            }
            else if (move_color == 1 && to_square >= 0 && to_square <= 7)
            {
                move |= 3 << 12;
            }

            moves[length] = move;
            length++;

            pawn_moves_bitbaord &= pawn_moves_bitbaord - 1;
        }

        pawns_bitboard &= pawns_bitboard - 1;
    }

    Bitboard knights_bitboard = position.piece_bitboards[move_color][1];

    while (knights_bitboard != 0)
    {
        int from_square = __builtin_ctzll(knights_bitboard);
        Bitboard knight_moves_bitbaord = get_knight_moves_bitboard(from_square, position);

        while (knight_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(knight_moves_bitbaord);
            Move move = (from_square << 6) | to_square;
            moves[length] = move;
            length++;

            knight_moves_bitbaord &= knight_moves_bitbaord - 1;
        }

        knights_bitboard &= knights_bitboard - 1;
    }

    Bitboard bishops_bitboard = position.piece_bitboards[move_color][2];

    while (bishops_bitboard != 0)
    {
        int from_square = __builtin_ctzll(bishops_bitboard);
        Bitboard bishop_moves_bitbaord = get_bishop_moves_bitboard(from_square, position);

        while (bishop_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(bishop_moves_bitbaord);
            Move move = (from_square << 6) | to_square;
            moves[length] = move;
            length++;

            bishop_moves_bitbaord &= bishop_moves_bitbaord - 1;
        }

        bishops_bitboard &= bishops_bitboard - 1;
    }

    Bitboard rooks_bitboard = position.piece_bitboards[move_color][3];

    while (rooks_bitboard != 0)
    {
        int from_square = __builtin_ctzll(rooks_bitboard);
        Bitboard rook_moves_bitbaord = get_rook_moves_bitboard(from_square, position);

        while (rook_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(rook_moves_bitbaord);
            Move move = (from_square << 6) | to_square;
            moves[length] = move;
            length++;

            rook_moves_bitbaord &= rook_moves_bitbaord - 1;
        }

        rooks_bitboard &= rooks_bitboard - 1;
    }

    Bitboard queens_bitboard = position.piece_bitboards[move_color][4];

    while (queens_bitboard != 0)
    {
        int from_square = __builtin_ctzll(queens_bitboard);
        Bitboard queen_moves_bitbaord = get_queen_moves_bitboard(from_square, position);

        while (queen_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(queen_moves_bitbaord);
            Move move = (from_square << 6) | to_square;
            moves[length] = move;
            length++;

            queen_moves_bitbaord &= queen_moves_bitbaord - 1;
        }

        queens_bitboard &= queens_bitboard - 1;
    }

    Bitboard kings_bitboard = position.piece_bitboards[move_color][5];

    while (kings_bitboard != 0)
    {
        int from_square = __builtin_ctzll(kings_bitboard);
        Bitboard king_moves_bitbaord = get_king_moves_bitboard(from_square, position);

        while (king_moves_bitbaord != 0)
        {
            int to_square = __builtin_ctzll(king_moves_bitbaord);
            Move move = (from_square << 6) | to_square;
            moves[length] = move;
            length++;

            king_moves_bitbaord &= king_moves_bitbaord - 1;
        }

        kings_bitboard &= kings_bitboard - 1;
    }

    if (position.color_to_move == 0)
    {
        if ((position.castling_rights & 0x1) != 0 && position.pieces[4] == 6 && position.pieces[5] == 0 && position.pieces[6] == 0 && position.pieces[7] == 4)
        {
            Move move = (2 << 12) | (4 << 6) | 6;
            moves[length] = move;
            length++;
        }
        else if ((position.castling_rights & 0x2) != 0 && position.pieces[4] == 6 && position.pieces[3] == 0 && position.pieces[2] == 0 && position.pieces[1] == 0 && position.pieces[0] == 4)
        {
            Move move = (2 << 12) | (4 << 6) | 2;
            moves[length] = move;
            length++;
        }
    }
    else
    {
        if ((position.castling_rights & 0x4) != 0 && position.pieces[60] == -6 && position.pieces[61] == 0 && position.pieces[62] == 0 && position.pieces[63] == -4)
        {
            Move move = (2 << 12) | (60 << 6) | 62;
            moves[length] = move;
            length++;
        }
        else if ((position.castling_rights & 0x8) != 0 && position.pieces[60] == -6 && position.pieces[59] == 0 && position.pieces[58] == 0 && position.pieces[57] == 0 && position.pieces[56] == -4)
        {
            Move move = (2 << 12) | (60 << 6) | 58;
            moves[length] = move;
            length++;
        }
    }
}

double search(Position &position, int depth, double alpha = std::numeric_limits<double>::lowest(),
              double beta = std::numeric_limits<double>::max())
{
    if (depth == 0)
    {
        return evaluate_position(position);
    }

    Move moves[256];
    int length = 0;
    generate_all_moves(moves, length, position);
    double best_eval = position.color_to_move == 0
                           ? -std::numeric_limits<double>::infinity()
                           : std::numeric_limits<double>::infinity();

    for (int i = 0; i < length; i++)
    {
        int to_square = moves[i] & 0x3f;
        Unmove unmove = {moves[i], position.pieces[to_square], position.castling_rights};

        make_move(moves[i], position);

        double eval = search(position, depth - 1, alpha, beta);

        unmake_move(unmove, position);

        if (position.color_to_move == 0)
        {
            if (eval > best_eval)
                best_eval = eval;
            if (best_eval > alpha)
                alpha = best_eval;
        }
        else
        {
            if (eval < best_eval)
                best_eval = eval;
            if (best_eval < beta)
                beta = best_eval;
        }

        if (alpha >= beta)
            break;
    }

    return best_eval;
}

Move get_best_move(Position &position, int depth, double alpha = std::numeric_limits<double>::lowest(),
                   double beta = std::numeric_limits<double>::max())
{
    Move moves[256];
    int length = 0;
    generate_all_moves(moves, length, position);
    double best_eval = position.color_to_move == 0
                           ? -std::numeric_limits<double>::infinity()
                           : std::numeric_limits<double>::infinity();
    Move best_move = moves[0];

    for (int i = 0; i < length; i++)
    {
        int to_square = moves[i] & 0x3f;
        Unmove unmove = {moves[i], position.pieces[to_square], position.castling_rights};

        make_move(moves[i], position);

        double eval = search(position, depth - 1);

        unmake_move(unmove, position);

        if (position.color_to_move == 0)
        {
            if (eval > best_eval)
            {
                best_eval = eval;
                best_move = moves[i];
            }
            if (best_eval > alpha)
                alpha = best_eval;
        }
        else
        {
            if (eval < best_eval)
            {
                best_move = moves[i];
                best_eval = eval;
            }
            if (best_eval < beta)
                beta = best_eval;
        }

        if (alpha >= beta)
            break;
    }

    return best_move;
}

char piece_to_fen_char(const Piece &piece)
{
    if (piece == 0)
    {
        return '0';
    }
    else if (piece == 1)
    {
        return 'P';
    }
    else if (piece == 2)
    {
        return 'N';
    }
    else if (piece == 3)
    {
        return 'B';
    }
    else if (piece == 4)
    {
        return 'R';
    }
    else if (piece == 5)
    {
        return 'Q';
    }
    else if (piece == 6)
    {
        return 'K';
    }
    else if (piece == -1)
    {
        return 'p';
    }
    else if (piece == -2)
    {
        return 'n';
    }
    else if (piece == -3)
    {
        return 'b';
    }
    else if (piece == -4)
    {
        return 'r';
    }
    else if (piece == -5)
    {
        return 'q';
    }
    else if (piece == -6)
    {
        return 'k';
    }

    return 'U';
}

std::string position_to_fen(const Position &position)
{
    std::string fen = "";

    for (int i = 7; i >= 0; i--)
    {
        for (int j = 0; j < 8; j++)
        {
            int square = i * 8 + j;
            char fen_char = piece_to_fen_char(position.pieces[square]);
            fen += fen_char;
        }
    }

    return fen;
}

int fen_char_to_piece_type(char character)
{
    if (character == 'P' || character == 'p')
    {
        return 0;
    }
    else if (character == 'N' || character == 'n')
    {
        return 1;
    }
    else if (character == 'B' || character == 'b')
    {
        return 2;
    }
    else if (character == 'R' || character == 'r')
    {
        return 3;
    }
    else if (character == 'Q' || character == 'q')
    {
        return 4;
    }
    else
    {
        return 5;
    }
}

int piece_to_color(const Piece &piece)
{
    return piece > 0 ? 0 : piece < 0 ? 1
                                     : -1;
}

Piece fen_char_to_piece(char fen_char)
{
    if (fen_char == '0')
    {
        return 0;
    }
    else if (fen_char == 'P')
    {
        return 1;
    }
    else if (fen_char == 'N')
    {
        return 2;
    }
    else if (fen_char == 'B')
    {
        return 3;
    }
    else if (fen_char == 'R')
    {
        return 4;
    }
    else if (fen_char == 'Q')
    {
        return 5;
    }
    else if (fen_char == 'K')
    {
        return 6;
    }
    else if (fen_char == 'p')
    {
        return -1;
    }
    else if (fen_char == 'n')
    {
        return -2;
    }
    else if (fen_char == 'b')
    {
        return -3;
    }
    else if (fen_char == 'r')
    {
        return -4;
    }
    else if (fen_char == 'q')
    {
        return -5;
    }
    else if (fen_char == 'k')
    {
        return -6;
    }

    return 0;
}

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void set_position(char *fen, int color_to_move)
    {
        position = {};
        position.color_to_move = color_to_move;
        char *current = fen;

        for (int i = 7; i >= 0; i--)
        {
            for (int j = 0; j < 8; j++)
            {
                int square = i * 8 + j;

                if (current[0] != '0')
                {
                    int color = current[0] >= 'A' && current[0] <= 'Z' ? 0 : 1;
                    int type = fen_char_to_piece_type(current[0]);

                    position.piece_bitboards[color][type] |= 1ULL << square;
                    position.occupancy_bitboards[color] |= 1ULL << square;

                    position.pieces[square] = fen_char_to_piece(current[0]);
                }

                current++;
            }
        }

        position.occupancy_bitboards[2] = position.occupancy_bitboards[0] | position.occupancy_bitboards[1];

        setup_knight_attacks_bitboards(knight_attacks_bitboards);
        setup_king_attacks_bitboards(king_attacks_bitboards);
    }

    EMSCRIPTEN_KEEPALIVE
    const char *play_move(int from_square, int to_square)
    {
        Move move = (from_square << 6) | to_square;

        // Assumes player is white
        if (position.pieces[from_square] == 1 && to_square >= 56 && to_square <= 63)
        {
            move |= 3 << 12;
        }

        if (position.pieces[from_square] == 6 && from_square == 4 && to_square == 6)
        {
            move |= 2 << 12;
        }
        else if (position.pieces[from_square] == 6 && from_square == 4 && to_square == 2)
        {
            move |= 2 << 12;
        }

        make_move(move, position);

        static std::string position_string;
        position_string = position_to_fen(position);
        return position_string.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    const char *play_best_move()
    {
        Move best_move = get_best_move(position, 7);
        make_move(best_move, position);

        static std::string position_string;
        position_string = position_to_fen(position);
        return position_string.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    const char *get_legal_moves(int from_square)
    {
        if (position.pieces[from_square] == 0)
        {
            return "";
        }

        if ((position.color_to_move == 0 && position.pieces[from_square] <= 0) || (position.color_to_move == 1 && position.pieces[from_square] >= 0))
        {
            return "";
        }

        static std::string legal_moves;
        legal_moves = "";
        int piece_type = piece_to_type(position.pieces[from_square]);

        Bitboard moves_bitboard = 0;

        if (piece_type == 0)
        {
            moves_bitboard = get_pawn_moves_bitboard(from_square, position);
        }
        else if (piece_type == 1)
        {
            moves_bitboard = get_knight_moves_bitboard(from_square, position);
        }
        else if (piece_type == 2)
        {
            moves_bitboard = get_bishop_moves_bitboard(from_square, position);
        }
        else if (piece_type == 3)
        {
            moves_bitboard = get_rook_moves_bitboard(from_square, position);
        }
        else if (piece_type == 4)
        {
            moves_bitboard = get_queen_moves_bitboard(from_square, position);
        }
        else if (piece_type == 5)
        {
            moves_bitboard = get_king_moves_bitboard(from_square, position);

            if ((position.castling_rights & 0x1) != 0 && position.pieces[4] == 6 && position.pieces[5] == 0 && position.pieces[6] == 0 && position.pieces[7] == 4)
            {
                moves_bitboard |= 1ULL << 6;
            }
            else if ((position.castling_rights & 0x2) != 0 && position.pieces[4] == 6 && position.pieces[3] == 0 && position.pieces[2] == 0 && position.pieces[1] == 0 && position.pieces[0] == 4)
            {
                moves_bitboard |= 1ULL << 2;
            }
        }

        while (moves_bitboard != 0)
        {
            int to_square = __builtin_ctzll(moves_bitboard);
            legal_moves += std::to_string(to_square) + " ";

            moves_bitboard &= moves_bitboard - 1;
        }

        return legal_moves.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    double get_eval()
    {
        return evaluate_position(position);
    }
}