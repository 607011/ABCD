(function (window) {
    "use strict";
    let el = {};

    const EMPTY = ".";
    const FADING_DURATION_MS = 200;
    const CELL_GAP = 2;
    const CELL_SIZE = 44;

    class ABCDGame extends HTMLElement {
        static observedAttributes = ["board"];
        constructor() {
            super();
            this._internals = this.attachInternals();
            this._board = null;
            this._style = null;
            this.moves = [];
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
    transition-timing-function: ease-out, ease-in;
    width: var(--cell-size);
    height: var(--cell-size);
}
.board > div.A {
    background-color: gold;
    border-radius: 25%;
}
.board > div.B {
    background-color: steelblue;
    border-radius: 50%;
}
.board > div.C {
    background-color: orangered;
    width: calc(var(--cell-size) * 0.8);
    transform: skewX(-10deg) translateX(calc(var(--cell-size) * 0.1));
}
.board > div.D {
    background-color: olivedrab;
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

        }
        attributeChangedCallback(name, _oldValue, newValue) {
            switch (name) {
                case "board":
                    this._board = newValue.split("\n").map((row => row.split("").map(letter => { return { letter }; })));
                    this._width = this._board[0].length;
                    this._height = this._board.length;
                    this.build();
                    this.moves = [];
                    break;
                default: break;
            }
        }

        build() {
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
                        this.removeLetter(row, col);
                        setTimeout(() => {
                            this.applyGravity();
                            if (this.boardIsClear()) {
                                const moves = this.moves.map(coords => `${coords.row},${coords.col}`).join(" ");
                                console.info(moves);
                                alert(`Geschafft mit ${this.moves.length} Zügen: ${moves}`);
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
                        let cell = this._board[row + emptyCount][col];
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

    }

    // best moves so far: 8,0 8,0 8,0 8,1 8,1 8,1 8,1 8,2 5,3 5,4 6,4 7,2 7,4 7,6
    function play(seq) {
        const moves = seq.split(" ").map(coords => coords.split(",").map(Number));
        let t0 = performance.now();
        const autoplay = _t => {
            if (moves.length > 0 && performance.now() > t0 + 1200) {
                el.game.click(...moves.shift());
                t0 = performance.now()
            }
            requestAnimationFrame(autoplay);
        }
        requestAnimationFrame(autoplay);
    }

    function main() {
        customElements.define("abcd-game", ABCDGame);
        el.game = document.querySelector("abcd-game");
        el.game.setAttribute("board", `AAACDCB
DCABAAB
DCCBCCB
BAACBCB
BABDDBC
BDBBBDC
BCCBAAB
ACACDAC
ABAAAAA`);
    }

    window.addEventListener("load", main);
    window.exports = {
        play
    };

})(window);
