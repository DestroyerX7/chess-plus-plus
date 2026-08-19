let Module: any = null;

self.onmessage = async (e) => {
  if (Module === null) {
    // @ts-expect-error
    const { default: createModule } = await import(/* webpackIgnore: true */ "/wasm/chess-engine.js");

    Module = await createModule({
      locateFile(path: string) {
        return `/wasm/${path}`;
      },
    });
  }

  const { type } = e.data;

  if (type === "setPosition") {
    Module.ccall("set_position", null, ["string", "number"], [e.data.fen, e.data.color]);

    self.postMessage({ type: "setPosition" });
  } else if (type === "playBestMove") {
    const fen: string = Module.ccall("play_best_move", "string", null, null);

    self.postMessage({ type: "playBestMove", fen });
  } else if (type === "getLegalMoves") {
    const legalMovesString: string = Module.ccall("get_legal_moves", "string", ["number"], [e.data.fromSquare]);
    const legalMoves = legalMovesString.split(" ").filter(s => s.length > 0).map(Number);
    self.postMessage({ type: "getLegalMoves", legalMoves });
  } else if (type === "playMove") {
    const fen: string = Module.ccall("play_move", "string", ["number", "number"], [e.data.fromSquare, e.data.toSquare]);

    self.postMessage({ type: "playMove", fen });
  }
};
