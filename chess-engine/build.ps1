em++ src/main.cpp -o ../web/public/wasm/chess-engine.js `
-s MODULARIZE=1 `
-s EXPORT_ES6=1 `
-s ENVIRONMENT=worker `
-s ALLOW_MEMORY_GROWTH=1 `
-s EXPORTED_FUNCTIONS='["_set_position, _play_best_move, _get_legal_moves, _play_move"]' `
-s EXPORTED_RUNTIME_METHODS='["ccall"]'