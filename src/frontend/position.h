#ifndef SYSY_COMPILER_FRONTEND_POSITION_H
#define SYSY_COMPILER_FRONTEND_POSITION_H

struct Position {
    size_t row;
    size_t col;
};

struct Range {
    Position begin;
    Position end;
};

template<typename Ty>
struct WithPosition {
    Ty value;
    Position position;

    WithPosition() = default;

    WithPosition(Ty value, Position position)
            : value(value), position(position) {}
};

#endif //SYSY_COMPILER_FRONTEND_POSITION_H
