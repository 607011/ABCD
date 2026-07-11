import("./mt.js");

(function (window) {
    "use strict";

    let el = {};

    const EMPTY = ".";
    const FADING_DURATION_MS = 200;
    const CELL_GAP = 2;
    const CELL_SIZE = 44;
    const LETTERS = ["A", "B", "C", "D"];

    class ABCDGame extends HTMLElement {
        static observedAttributes = ["board", "seed", "width", "height"];
        constructor() {
            super();
            this._internals = this.attachInternals();
            this._board = null;
            this._style = null;
            this._moves = [];
            this._rng = new MersenneTwister;
            this._seed = 1;
            this._width = 0;
            this._height = 0;
        }
        /**
         * @param {number} newSeed
         */
        set seed(newSeed) {
            this._seed = newSeed;
            this.generate();
            this.reset();
        }
        get seed() {
            return this._seed;
        }
        get moves() {
            return this._moves;
        }
        set moves(newMoves) {
            this._moves = newMoves;
            this.dispatchMovesChanged();
        }
        get width() {
            return this._width;
        }
        get height() {
            return this._height;
        }
        dispatchMovesChanged() {
            this.dispatchEvent(new CustomEvent("moveschanged", {
                detail: { moves: this.moves }
            }));
        }
        connectedCallback() {
            this._shadow = this.attachShadow({ mode: "open" });
            this._style = document.createElement("style");
            this._style.textContent = `
:host {
    --cell-size: ${CELL_SIZE}px;
    --fading-duration: ${this.getAttribute("fading-duration-ms") || 0}ms;
}
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}
body {
    background-color: #222;
    padding: 8px;
    font-family: Helvetica, Arial, sans-serif;
}
.board {
    position: relative;
    display: block;
    margin: 0 auto;
}
.board > div {
    position: absolute;
    cursor: pointer;
    text-align: center;
    transition-property: transform, top;
    transition-delay: 0ms, var(--fading-duration);
    transition-duration: var(--fading-duration), 0ms;
    transition-timing-function: ease-in-out, cubic-bezier(1, 0, 0.9, 1.29);
    width: var(--cell-size);
    height: var(--cell-size);
}
.board > div.A {
    background-color: gold;
    border-radius: 25%;
    transform: scale(0.98);
}
.board > div.B {
    background-color: steelblue;
    border-radius: 50%;
}
.board > div.C {
    background-color: orangered;
    width: calc(var(--cell-size) * 0.82);
    transform: skewX(-10deg) translateX(calc(var(--cell-size) * 0.1));
}
.board > div.D {
    background-color: olivedrab;
    transform: scale(0.95);
}
.board > div.fade {
    transform: scale(0);
}
`;
            this._shadow.appendChild(this._style);
            this._body = document.createElement("body");
            this._boardEl = document.createElement("div");
            this._boardEl.className = "board";
            this._body.appendChild(this._boardEl);
            this._shadow.appendChild(this._body);
            this.generate();
            this.reset();
        }
        attributeChangedCallback(name, _oldValue, newValue) {
            switch (name) {
                case "seed":
                    this._seed = parseInt(newValue);
                    if (!isNaN(this._seed)) {
                        this.generate();
                        this.reset();
                    }
                    break;
                case "width":
                    this._width = parseInt(newValue);
                    if (!isNaN(this._width)) {
                        this.generate();
                        this.reset();
                    }
                    break;
                case "height":
                    this._height = parseInt(newValue);
                    if (!isNaN(this._height)) {
                        this.generate();
                        this.reset();
                    }
                    break;
                case "board":
                    this._board = newValue.split("\n").map((row => row.split("").map(letter => { return { letter }; })));
                    this._width = this._board[0].length;
                    this._height = this._board.length;
                    this.reset();
                    break;
                default:
                    break;
            }
        }

        reset() {
            this.moves = [];
            this.build();
        }

        restart() {
            this.generate();
            this.reset();
        }

        build() {
            if (!this._board || !this._body)
                return;
            let cells = [];
            for (const [row, rowObjs] of this._board.entries()) {
                for (const [col, cell] of rowObjs.entries()) {
                    const div = document.createElement("div");
                    div.className = cell.letter;
                    div.style.top = `${(CELL_SIZE + CELL_GAP) * row}px`;
                    div.style.left = `${(CELL_SIZE + CELL_GAP) * col}px`;
                    div.setAttribute("data-row", row);
                    div.setAttribute("data-col", col);
                    this._board[row][col].el = div;
                    div.addEventListener("click", e => {
                        const row = parseInt(e.target.getAttribute("data-row"));
                        const col = parseInt(e.target.getAttribute("data-col"));
                        this.moves.push({ row, col });
                        this.dispatchMovesChanged();
                        this.removeLetter(row, col);
                        setTimeout(() => {
                            this.applyGravity();
                            if (this.boardIsClear()) {
                                const moves = this.moves.map(coords => `${coords.row},${coords.col}`).join(" ");
                                console.info(moves);
                                alert(`Geschafft mit ${this.moves.length} Zügen: ${moves}`);
                                this.generate();
                                this.reset();
                            }
                        }, parseInt(this.getAttribute("fading-duration-ms") || FADING_DURATION_MS));
                    });
                    cells.push(div);
                }
            }
            this._boardEl.replaceChildren(...cells);
            this._boardEl.style.width = `${this._width * ((CELL_SIZE + CELL_GAP))}px`;
            this._boardEl.style.height = `${this._height * ((CELL_SIZE + CELL_GAP))}px`;
        }

        boardIsClear() {
            return this._board.every(row => row.every(cell => cell.letter === EMPTY));
        }

        applyGravity() {
            for (let col = 0; col < this._board[0].length; ++col) {
                let emptyCount = 0;
                for (let row = this._height - 1; row >= 0; --row) {
                    const cell = this._board[row][col];
                    if (cell.letter === EMPTY) {
                        ++emptyCount;
                    }
                    else if (emptyCount > 0) {
                        this._board[row + emptyCount][col] = this._board[row][col];
                        this._board[row][col] = { letter: EMPTY };
                        const cell = this._board[row + emptyCount][col];
                        cell.el.setAttribute("data-row", row + emptyCount);
                        cell.el.style.transitionDuration = `${(emptyCount) * 100}ms`;
                        cell.el.style.top = `${(row + emptyCount) * (CELL_SIZE + CELL_GAP)}px`;
                    }
                }
            }
            this._boardEl.querySelectorAll(".fade").forEach(cell => cell.remove());
        }

        removeLetter(row, col) {
            const letter = this._board[row][col].letter;
            const remove = (r, c) => {
                const cell = this._board[r][c];
                if (cell.letter === letter) {
                    cell.letter = EMPTY;
                    cell.el.classList.add("fade");
                    if (r > 0)
                        remove(r - 1, c);
                    if (r < this._height - 1)
                        remove(r + 1, c);
                    if (c > 0)
                        remove(r, c - 1);
                    if (c < this._width - 1)
                        remove(r, c + 1);
                }
            };
            remove(row, col);
        }

        click(row, col) {
            this.removeLetter(row, col);
            setTimeout(() => {
                this.applyGravity();
            }, parseInt(this.getAttribute("fading-duration-ms") || FADING_DURATION_MS));
        }

        generate() {
            this._rng.init_genrand(this._seed);
            this._board = [...Array(this._height)].map(_ => Array(this._width).fill(null));
            for (let row = 0; row < this._height; ++row) {
                for (let col = 0; col < this._width; ++col) {
                    this._board[row][col] = {
                        letter: LETTERS[Math.floor(this._rng.genrand_int31() % LETTERS.length)],
                    }
                }
            }
            console.log(this._board.map(row => row.map(cell => cell.letter).join("")).join("\n"));
            this.build();
        }

    }

    /*
    best solutions so far:
    seed=3479834 => 11 moves: 5,2 7,5 6,0 5,0 6,1 6,4 7,4 8,0 8,2 7,5 8,5
                    11 moves: 5,4 5,4 8,5 8,5 8,5 8,6 6,1 7,0 7,0 7,1 8,1
                    11 moves: 4,4 5,2 7,5 8,5 8,5 8,6 6,0 7,1 8,0 6,0 7,1
                    10 moves: 7,0 5,2 7,3 7,5 7,4 8,0 6,0 8,5 8,3 8,4
                    10 moves: 5,3 5,5 4,1 7,3 7,5 8,5 8,4 8,1 8,0 8,0
                    10 moves: 4,2 8,6 4,1 7,3 7,4 8,1 8,0 8,5 8,4 8,0
                    10 moves: 6,0 5,1 7,2 7,5 7,4 7,5 8,5 8,0 6,0 7,1
    */
    function play(seq) {
        if (el.game.boardIsClear())
            el.game.restart();
        const moves = seq.split(" ").map(coords => coords.split(",").map(Number));
        let index = 0;
        let executedMoves = [];

        const nextClick = () => {
            if (index < moves.length) {
                const [row, col] = moves[index];

                // Find and click the actual DOM cell to trigger full flow (including sound/effects and dispatching)
                const cellEl = el.game.shadowRoot.querySelector(`div[data-row="${row}"][data-col="${col}"]`);
                if (cellEl) {
                    cellEl.click();
                } else {
                    // Fallback to custom click if element is not found (e.g. because of layout changing)
                    el.game.click(row, col);
                }

                index++;
                // Wait for cells to fade out and gravity transition before triggering the next click (approx. 1200ms)
                setTimeout(nextClick, 1200);
            }
        };
        nextClick();
    }

    function parseHash(hash) {
        for (const arg of hash.split(";")) {
            const [key, value] = arg.split("=");
            switch (key) {
                case "game":
                    const seed = parseInt(value) || 0;
                    el.game.setAttribute("seed", seed);
                    el.seed.value = seed;
                    break;
                case "width":
                    const width = parseInt(value) || 0;
                    el.game.setAttribute("width", width);
                    break;
                case "height":
                    const height = parseInt(value) || 0;
                    el.game.setAttribute("height", height);
                    break;
                default:
                    console.error(`invalid hash param: ${key}=${value}`);
                    break;
            }
        }
    }

    function onHashChange(_event) {
        parseHash(window.location.hash.substring(1));
    }

    function buildHash() {
        window.location.hash = `#game=${el.game.seed};width=${el.game.width};height=${el.game.height}`;
    }

    function onMovesChanged(e) {
        el.moveCount.textContent = `${e.detail.moves.length} ${e.detail.moves.length === 1 ? "move" : "moves"}`;
    }

    function main() {
        customElements.define("abcd-game", ABCDGame);
        el.game = document.querySelector("abcd-game");
        el.game.addEventListener("moveschanged", onMovesChanged);
        el.seed = document.querySelector("#seed");
        el.seed.addEventListener("change", e => {
            el.game.setAttribute("seed", e.target.value);
            buildHash();
        });
        el.moveCount = document.querySelector("#move-count");
        el.restart = document.querySelector("#restart");
        el.restart.addEventListener("click", () => el.game.restart());
        window.addEventListener("hashchange", onHashChange);
        if (window.location.hash.length < 1) {
            window.location.hash = "#game=3479834;width=7;height=9";
        }
        onHashChange();
    }

    window.addEventListener("load", main);
    window.exports = {
        play
    };

})(window);
