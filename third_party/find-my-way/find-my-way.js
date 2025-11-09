var __getOwnPropNames = Object.getOwnPropertyNames;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};

// assert-stub.js
var require_assert_stub = __commonJS({
  "assert-stub.js"(exports, module) {
    function assert(condition, message) {
      if (!condition) {
        throw new Error(message || "Assertion failed");
      }
    }
    assert.ok = assert;
    assert.strictEqual = function(a, b, message) {
      if (a !== b) {
        throw new Error(message || `Expected ${a} to equal ${b}`);
      }
    };
    module.exports = assert;
  }
});

// node_modules/fast-decode-uri-component/index.js
var require_fast_decode_uri_component = __commonJS({
  "node_modules/fast-decode-uri-component/index.js"(exports, module) {
    "use strict";
    var UTF8_ACCEPT = 12;
    var UTF8_REJECT = 0;
    var UTF8_DATA = [
      // The first part of the table maps bytes to character to a transition.
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      2,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      3,
      4,
      4,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      5,
      6,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      7,
      8,
      7,
      7,
      10,
      9,
      9,
      9,
      11,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      // The second part of the table maps a state to a new state when adding a
      // transition.
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      12,
      0,
      0,
      0,
      0,
      24,
      36,
      48,
      60,
      72,
      84,
      96,
      0,
      12,
      12,
      12,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      24,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      24,
      24,
      24,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      24,
      24,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      48,
      48,
      48,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      48,
      48,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      48,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      // The third part maps the current transition to a mask that needs to apply
      // to the byte.
      127,
      63,
      63,
      63,
      0,
      31,
      15,
      15,
      15,
      7,
      7,
      7
    ];
    function decodeURIComponent(uri) {
      var percentPosition = uri.indexOf("%");
      if (percentPosition === -1) return uri;
      var length = uri.length;
      var decoded = "";
      var last = 0;
      var codepoint = 0;
      var startOfOctets = percentPosition;
      var state = UTF8_ACCEPT;
      while (percentPosition > -1 && percentPosition < length) {
        var high = hexCodeToInt(uri[percentPosition + 1], 4);
        var low = hexCodeToInt(uri[percentPosition + 2], 0);
        var byte = high | low;
        var type = UTF8_DATA[byte];
        state = UTF8_DATA[256 + state + type];
        codepoint = codepoint << 6 | byte & UTF8_DATA[364 + type];
        if (state === UTF8_ACCEPT) {
          decoded += uri.slice(last, startOfOctets);
          decoded += codepoint <= 65535 ? String.fromCharCode(codepoint) : String.fromCharCode(
            55232 + (codepoint >> 10),
            56320 + (codepoint & 1023)
          );
          codepoint = 0;
          last = percentPosition + 3;
          percentPosition = startOfOctets = uri.indexOf("%", last);
        } else if (state === UTF8_REJECT) {
          return null;
        } else {
          percentPosition += 3;
          if (percentPosition < length && uri.charCodeAt(percentPosition) === 37) continue;
          return null;
        }
      }
      return decoded + uri.slice(last);
    }
    var HEX = {
      "0": 0,
      "1": 1,
      "2": 2,
      "3": 3,
      "4": 4,
      "5": 5,
      "6": 6,
      "7": 7,
      "8": 8,
      "9": 9,
      "a": 10,
      "A": 10,
      "b": 11,
      "B": 11,
      "c": 12,
      "C": 12,
      "d": 13,
      "D": 13,
      "e": 14,
      "E": 14,
      "f": 15,
      "F": 15
    };
    function hexCodeToInt(c, shift) {
      var i = HEX[c];
      return i === void 0 ? 255 : i << shift;
    }
    module.exports = decodeURIComponent;
  }
});

// node_modules/fast-querystring/lib/parse.js
var require_parse = __commonJS({
  "node_modules/fast-querystring/lib/parse.js"(exports, module) {
    "use strict";
    var fastDecode = require_fast_decode_uri_component();
    var plusRegex = /\+/g;
    var Empty = function() {
    };
    Empty.prototype = /* @__PURE__ */ Object.create(null);
    function parse(input) {
      const result = new Empty();
      if (typeof input !== "string") {
        return result;
      }
      let inputLength = input.length;
      let key = "";
      let value = "";
      let startingIndex = -1;
      let equalityIndex = -1;
      let shouldDecodeKey = false;
      let shouldDecodeValue = false;
      let keyHasPlus = false;
      let valueHasPlus = false;
      let hasBothKeyValuePair = false;
      let c = 0;
      for (let i = 0; i < inputLength + 1; i++) {
        c = i !== inputLength ? input.charCodeAt(i) : 38;
        if (c === 38) {
          hasBothKeyValuePair = equalityIndex > startingIndex;
          if (!hasBothKeyValuePair) {
            equalityIndex = i;
          }
          key = input.slice(startingIndex + 1, equalityIndex);
          if (hasBothKeyValuePair || key.length > 0) {
            if (keyHasPlus) {
              key = key.replace(plusRegex, " ");
            }
            if (shouldDecodeKey) {
              key = fastDecode(key) || key;
            }
            if (hasBothKeyValuePair) {
              value = input.slice(equalityIndex + 1, i);
              if (valueHasPlus) {
                value = value.replace(plusRegex, " ");
              }
              if (shouldDecodeValue) {
                value = fastDecode(value) || value;
              }
            }
            const currentValue = result[key];
            if (currentValue === void 0) {
              result[key] = value;
            } else {
              if (currentValue.pop) {
                currentValue.push(value);
              } else {
                result[key] = [currentValue, value];
              }
            }
          }
          value = "";
          startingIndex = i;
          equalityIndex = i;
          shouldDecodeKey = false;
          shouldDecodeValue = false;
          keyHasPlus = false;
          valueHasPlus = false;
        } else if (c === 61) {
          if (equalityIndex <= startingIndex) {
            equalityIndex = i;
          } else {
            shouldDecodeValue = true;
          }
        } else if (c === 43) {
          if (equalityIndex > startingIndex) {
            valueHasPlus = true;
          } else {
            keyHasPlus = true;
          }
        } else if (c === 37) {
          if (equalityIndex > startingIndex) {
            shouldDecodeValue = true;
          } else {
            shouldDecodeKey = true;
          }
        }
      }
      return result;
    }
    module.exports = parse;
  }
});

// node_modules/fast-querystring/lib/internals/querystring.js
var require_querystring = __commonJS({
  "node_modules/fast-querystring/lib/internals/querystring.js"(exports, module) {
    var hexTable = Array.from(
      { length: 256 },
      (_, i) => "%" + ((i < 16 ? "0" : "") + i.toString(16)).toUpperCase()
    );
    var noEscape = new Int8Array([
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      // 0 - 15
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      // 16 - 31
      0,
      1,
      0,
      0,
      0,
      0,
      0,
      1,
      1,
      1,
      1,
      0,
      0,
      1,
      1,
      0,
      // 32 - 47
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      0,
      0,
      0,
      0,
      0,
      0,
      // 48 - 63
      0,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      // 64 - 79
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      0,
      0,
      0,
      0,
      1,
      // 80 - 95
      0,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      // 96 - 111
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      1,
      0,
      0,
      0,
      1,
      0
      // 112 - 127
    ]);
    function encodeString(str) {
      const len = str.length;
      if (len === 0) return "";
      let out = "";
      let lastPos = 0;
      let i = 0;
      outer: for (; i < len; i++) {
        let c = str.charCodeAt(i);
        while (c < 128) {
          if (noEscape[c] !== 1) {
            if (lastPos < i) out += str.slice(lastPos, i);
            lastPos = i + 1;
            out += hexTable[c];
          }
          if (++i === len) break outer;
          c = str.charCodeAt(i);
        }
        if (lastPos < i) out += str.slice(lastPos, i);
        if (c < 2048) {
          lastPos = i + 1;
          out += hexTable[192 | c >> 6] + hexTable[128 | c & 63];
          continue;
        }
        if (c < 55296 || c >= 57344) {
          lastPos = i + 1;
          out += hexTable[224 | c >> 12] + hexTable[128 | c >> 6 & 63] + hexTable[128 | c & 63];
          continue;
        }
        ++i;
        if (i >= len) {
          throw new Error("URI malformed");
        }
        const c2 = str.charCodeAt(i) & 1023;
        lastPos = i + 1;
        c = 65536 + ((c & 1023) << 10 | c2);
        out += hexTable[240 | c >> 18] + hexTable[128 | c >> 12 & 63] + hexTable[128 | c >> 6 & 63] + hexTable[128 | c & 63];
      }
      if (lastPos === 0) return str;
      if (lastPos < len) return out + str.slice(lastPos);
      return out;
    }
    module.exports = { encodeString };
  }
});

// node_modules/fast-querystring/lib/stringify.js
var require_stringify = __commonJS({
  "node_modules/fast-querystring/lib/stringify.js"(exports, module) {
    "use strict";
    var { encodeString } = require_querystring();
    function getAsPrimitive(value) {
      const type = typeof value;
      if (type === "string") {
        return encodeString(value);
      } else if (type === "bigint") {
        return value.toString();
      } else if (type === "boolean") {
        return value ? "true" : "false";
      } else if (type === "number" && Number.isFinite(value)) {
        return value < 1e21 ? "" + value : encodeString("" + value);
      }
      return "";
    }
    function stringify(input) {
      let result = "";
      if (input === null || typeof input !== "object") {
        return result;
      }
      const separator = "&";
      const keys = Object.keys(input);
      const keyLength = keys.length;
      let valueLength = 0;
      for (let i = 0; i < keyLength; i++) {
        const key = keys[i];
        const value = input[key];
        const encodedKey = encodeString(key) + "=";
        if (i) {
          result += separator;
        }
        if (Array.isArray(value)) {
          valueLength = value.length;
          for (let j = 0; j < valueLength; j++) {
            if (j) {
              result += separator;
            }
            result += encodedKey;
            result += getAsPrimitive(value[j]);
          }
        } else {
          result += encodedKey;
          result += getAsPrimitive(value);
        }
      }
      return result;
    }
    module.exports = stringify;
  }
});

// node_modules/fast-querystring/lib/index.js
var require_lib = __commonJS({
  "node_modules/fast-querystring/lib/index.js"(exports, module) {
    "use strict";
    var parse = require_parse();
    var stringify = require_stringify();
    var fastQuerystring = {
      parse,
      stringify
    };
    module.exports = fastQuerystring;
    module.exports.default = fastQuerystring;
    module.exports.parse = parse;
    module.exports.stringify = stringify;
  }
});

// node_modules/ret/dist/types/tokens.js
var require_tokens = __commonJS({
  "node_modules/ret/dist/types/tokens.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
  }
});

// node_modules/ret/dist/types/types.js
var require_types = __commonJS({
  "node_modules/ret/dist/types/types.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.types = void 0;
    var types;
    (function(types2) {
      types2[types2["ROOT"] = 0] = "ROOT";
      types2[types2["GROUP"] = 1] = "GROUP";
      types2[types2["POSITION"] = 2] = "POSITION";
      types2[types2["SET"] = 3] = "SET";
      types2[types2["RANGE"] = 4] = "RANGE";
      types2[types2["REPETITION"] = 5] = "REPETITION";
      types2[types2["REFERENCE"] = 6] = "REFERENCE";
      types2[types2["CHAR"] = 7] = "CHAR";
    })(types = exports.types || (exports.types = {}));
  }
});

// node_modules/ret/dist/types/set-lookup.js
var require_set_lookup = __commonJS({
  "node_modules/ret/dist/types/set-lookup.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
  }
});

// node_modules/ret/dist/types/index.js
var require_types2 = __commonJS({
  "node_modules/ret/dist/types/index.js"(exports) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __exportStar = exports && exports.__exportStar || function(m, exports2) {
      for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports2, p)) __createBinding(exports2, m, p);
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    __exportStar(require_tokens(), exports);
    __exportStar(require_types(), exports);
    __exportStar(require_set_lookup(), exports);
  }
});

// node_modules/ret/dist/sets.js
var require_sets = __commonJS({
  "node_modules/ret/dist/sets.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.anyChar = exports.notWhitespace = exports.whitespace = exports.notInts = exports.ints = exports.notWords = exports.words = void 0;
    var types_1 = require_types2();
    var INTS = () => [{ type: types_1.types.RANGE, from: 48, to: 57 }];
    var WORDS = () => [
      { type: types_1.types.CHAR, value: 95 },
      { type: types_1.types.RANGE, from: 97, to: 122 },
      { type: types_1.types.RANGE, from: 65, to: 90 },
      { type: types_1.types.RANGE, from: 48, to: 57 }
    ];
    var WHITESPACE = () => [
      { type: types_1.types.CHAR, value: 9 },
      { type: types_1.types.CHAR, value: 10 },
      { type: types_1.types.CHAR, value: 11 },
      { type: types_1.types.CHAR, value: 12 },
      { type: types_1.types.CHAR, value: 13 },
      { type: types_1.types.CHAR, value: 32 },
      { type: types_1.types.CHAR, value: 160 },
      { type: types_1.types.CHAR, value: 5760 },
      { type: types_1.types.RANGE, from: 8192, to: 8202 },
      { type: types_1.types.CHAR, value: 8232 },
      { type: types_1.types.CHAR, value: 8233 },
      { type: types_1.types.CHAR, value: 8239 },
      { type: types_1.types.CHAR, value: 8287 },
      { type: types_1.types.CHAR, value: 12288 },
      { type: types_1.types.CHAR, value: 65279 }
    ];
    var NOTANYCHAR = () => [
      { type: types_1.types.CHAR, value: 10 },
      { type: types_1.types.CHAR, value: 13 },
      { type: types_1.types.CHAR, value: 8232 },
      { type: types_1.types.CHAR, value: 8233 }
    ];
    exports.words = () => ({ type: types_1.types.SET, set: WORDS(), not: false });
    exports.notWords = () => ({ type: types_1.types.SET, set: WORDS(), not: true });
    exports.ints = () => ({ type: types_1.types.SET, set: INTS(), not: false });
    exports.notInts = () => ({ type: types_1.types.SET, set: INTS(), not: true });
    exports.whitespace = () => ({ type: types_1.types.SET, set: WHITESPACE(), not: false });
    exports.notWhitespace = () => ({ type: types_1.types.SET, set: WHITESPACE(), not: true });
    exports.anyChar = () => ({ type: types_1.types.SET, set: NOTANYCHAR(), not: true });
  }
});

// node_modules/ret/dist/util.js
var require_util = __commonJS({
  "node_modules/ret/dist/util.js"(exports) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __setModuleDefault = exports && exports.__setModuleDefault || (Object.create ? (function(o, v) {
      Object.defineProperty(o, "default", { enumerable: true, value: v });
    }) : function(o, v) {
      o["default"] = v;
    });
    var __importStar = exports && exports.__importStar || function(mod) {
      if (mod && mod.__esModule) return mod;
      var result = {};
      if (mod != null) {
        for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
      }
      __setModuleDefault(result, mod);
      return result;
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.tokenizeClass = exports.strToChars = void 0;
    var types_1 = require_types2();
    var sets = __importStar(require_sets());
    var CTRL = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^ ?";
    exports.strToChars = (str) => {
      const charsRegex = /(\[\\b\])|(\\)?\\(?:u([A-F0-9]{4})|x([A-F0-9]{2})|c([@A-Z[\\\]^?])|([0tnvfr]))/g;
      return str.replace(charsRegex, (s, b, lbs, a16, b16, dctrl, eslsh) => {
        if (lbs) {
          return s;
        }
        let code = b ? 8 : a16 ? parseInt(a16, 16) : b16 ? parseInt(b16, 16) : dctrl ? CTRL.indexOf(dctrl) : {
          0: 0,
          t: 9,
          n: 10,
          v: 11,
          f: 12,
          r: 13
        }[eslsh];
        let c = String.fromCharCode(code);
        return /[[\]{}^$.|?*+()]/.test(c) ? `\\${c}` : c;
      });
    };
    exports.tokenizeClass = (str, regexpStr) => {
      var _a, _b, _c, _d, _e, _f, _g;
      let tokens = [], rs, c;
      const regexp = /\\(?:(w)|(d)|(s)|(W)|(D)|(S))|((?:(?:\\)(.)|([^\]\\]))-(((?:\\)])|(((?:\\)?([^\]])))))|(\])|(?:\\)?([^])/g;
      while ((rs = regexp.exec(str)) !== null) {
        const p = (_g = (_f = (_e = (_d = (_c = (_b = (_a = rs[1] && sets.words()) !== null && _a !== void 0 ? _a : rs[2] && sets.ints()) !== null && _b !== void 0 ? _b : rs[3] && sets.whitespace()) !== null && _c !== void 0 ? _c : rs[4] && sets.notWords()) !== null && _d !== void 0 ? _d : rs[5] && sets.notInts()) !== null && _e !== void 0 ? _e : rs[6] && sets.notWhitespace()) !== null && _f !== void 0 ? _f : rs[7] && {
          type: types_1.types.RANGE,
          from: (rs[8] || rs[9]).charCodeAt(0),
          to: (c = rs[10]).charCodeAt(c.length - 1)
        }) !== null && _g !== void 0 ? _g : (c = rs[16]) && { type: types_1.types.CHAR, value: c.charCodeAt(0) };
        if (p) {
          tokens.push(p);
        } else {
          return [tokens, regexp.lastIndex];
        }
      }
      throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Unterminated character class`);
    };
  }
});

// node_modules/ret/dist/tokenizer.js
var require_tokenizer = __commonJS({
  "node_modules/ret/dist/tokenizer.js"(exports) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __setModuleDefault = exports && exports.__setModuleDefault || (Object.create ? (function(o, v) {
      Object.defineProperty(o, "default", { enumerable: true, value: v });
    }) : function(o, v) {
      o["default"] = v;
    });
    var __importStar = exports && exports.__importStar || function(mod) {
      if (mod && mod.__esModule) return mod;
      var result = {};
      if (mod != null) {
        for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
      }
      __setModuleDefault(result, mod);
      return result;
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.tokenizer = void 0;
    var util = __importStar(require_util());
    var types_1 = require_types2();
    var sets = __importStar(require_sets());
    var captureGroupFirstChar = /^[a-zA-Z_$]$/i;
    var captureGroupChars = /^[a-zA-Z0-9_$]$/i;
    var digit = /\d/;
    exports.tokenizer = (regexpStr) => {
      let i = 0, c;
      let start = { type: types_1.types.ROOT, stack: [] };
      let lastGroup = start;
      let last = start.stack;
      let groupStack = [];
      let referenceQueue = [];
      let groupCount = 0;
      const repeatErr = (col) => {
        throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Nothing to repeat at column ${col - 1}`);
      };
      let str = util.strToChars(regexpStr);
      while (i < str.length) {
        switch (c = str[i++]) {
          // Handle escaped characters, inclues a few sets.
          case "\\":
            if (i === str.length) {
              throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: \\ at end of pattern`);
            }
            switch (c = str[i++]) {
              case "b":
                last.push({ type: types_1.types.POSITION, value: "b" });
                break;
              case "B":
                last.push({ type: types_1.types.POSITION, value: "B" });
                break;
              case "w":
                last.push(sets.words());
                break;
              case "W":
                last.push(sets.notWords());
                break;
              case "d":
                last.push(sets.ints());
                break;
              case "D":
                last.push(sets.notInts());
                break;
              case "s":
                last.push(sets.whitespace());
                break;
              case "S":
                last.push(sets.notWhitespace());
                break;
              default:
                if (digit.test(c)) {
                  let digits = c;
                  while (i < str.length && digit.test(str[i])) {
                    digits += str[i++];
                  }
                  let value = parseInt(digits, 10);
                  const reference = { type: types_1.types.REFERENCE, value };
                  last.push(reference);
                  referenceQueue.push({ reference, stack: last, index: last.length - 1 });
                } else {
                  last.push({ type: types_1.types.CHAR, value: c.charCodeAt(0) });
                }
            }
            break;
          // Positionals.
          case "^":
            last.push({ type: types_1.types.POSITION, value: "^" });
            break;
          case "$":
            last.push({ type: types_1.types.POSITION, value: "$" });
            break;
          // Handle custom sets.
          case "[": {
            let not;
            if (str[i] === "^") {
              not = true;
              i++;
            } else {
              not = false;
            }
            let classTokens = util.tokenizeClass(str.slice(i), regexpStr);
            i += classTokens[1];
            last.push({
              type: types_1.types.SET,
              set: classTokens[0],
              not
            });
            break;
          }
          // Class of any character except \n.
          case ".":
            last.push(sets.anyChar());
            break;
          // Push group onto stack.
          case "(": {
            let group = {
              type: types_1.types.GROUP,
              stack: [],
              remember: true
            };
            if (str[i] === "?") {
              c = str[i + 1];
              i += 2;
              if (c === "=") {
                group.followedBy = true;
                group.remember = false;
              } else if (c === "!") {
                group.notFollowedBy = true;
                group.remember = false;
              } else if (c === "<") {
                let name = "";
                if (captureGroupFirstChar.test(str[i])) {
                  name += str[i];
                  i++;
                } else {
                  throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Invalid capture group name, character '${str[i]}' after '<' at column ${i + 1}`);
                }
                while (i < str.length && captureGroupChars.test(str[i])) {
                  name += str[i];
                  i++;
                }
                if (!name) {
                  throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Invalid capture group name, character '${str[i]}' after '<' at column ${i + 1}`);
                }
                if (str[i] !== ">") {
                  throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Unclosed capture group name, expected '>', found '${str[i]}' at column ${i + 1}`);
                }
                group.name = name;
                i++;
              } else if (c === ":") {
                group.remember = false;
              } else {
                throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Invalid group, character '${c}' after '?' at column ${i - 1}`);
              }
            } else {
              groupCount += 1;
            }
            last.push(group);
            groupStack.push(lastGroup);
            lastGroup = group;
            last = group.stack;
            break;
          }
          // Pop group out of stack.
          case ")":
            if (groupStack.length === 0) {
              throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Unmatched ) at column ${i - 1}`);
            }
            lastGroup = groupStack.pop();
            last = lastGroup.options ? lastGroup.options[lastGroup.options.length - 1] : lastGroup.stack;
            break;
          // Use pipe character to give more choices.
          case "|": {
            if (!lastGroup.options) {
              lastGroup.options = [lastGroup.stack];
              delete lastGroup.stack;
            }
            let stack = [];
            lastGroup.options.push(stack);
            last = stack;
            break;
          }
          // Repetition.
          // For every repetition, remove last element from last stack
          // then insert back a RANGE object.
          // This design is chosen because there could be more than
          // one repetition symbols in a regex i.e. `a?+{2,3}`.
          case "{": {
            let rs = /^(\d+)(,(\d+)?)?\}/.exec(str.slice(i)), min, max;
            if (rs !== null) {
              if (last.length === 0) {
                repeatErr(i);
              }
              min = parseInt(rs[1], 10);
              max = rs[2] ? rs[3] ? parseInt(rs[3], 10) : Infinity : min;
              i += rs[0].length;
              last.push({
                type: types_1.types.REPETITION,
                min,
                max,
                value: last.pop()
              });
            } else {
              last.push({
                type: types_1.types.CHAR,
                value: 123
              });
            }
            break;
          }
          case "?":
            if (last.length === 0) {
              repeatErr(i);
            }
            last.push({
              type: types_1.types.REPETITION,
              min: 0,
              max: 1,
              value: last.pop()
            });
            break;
          case "+":
            if (last.length === 0) {
              repeatErr(i);
            }
            last.push({
              type: types_1.types.REPETITION,
              min: 1,
              max: Infinity,
              value: last.pop()
            });
            break;
          case "*":
            if (last.length === 0) {
              repeatErr(i);
            }
            last.push({
              type: types_1.types.REPETITION,
              min: 0,
              max: Infinity,
              value: last.pop()
            });
            break;
          // Default is a character that is not `\[](){}?+*^$`.
          default:
            last.push({
              type: types_1.types.CHAR,
              value: c.charCodeAt(0)
            });
        }
      }
      if (groupStack.length !== 0) {
        throw new SyntaxError(`Invalid regular expression: /${regexpStr}/: Unterminated group`);
      }
      updateReferences(referenceQueue, groupCount);
      return start;
    };
    function updateReferences(referenceQueue, groupCount) {
      for (const elem of referenceQueue.reverse()) {
        if (groupCount < elem.reference.value) {
          elem.reference.type = types_1.types.CHAR;
          const valueString = elem.reference.value.toString();
          elem.reference.value = parseInt(valueString, 8);
          if (!/^[0-7]+$/.test(valueString)) {
            let i = 0;
            while (valueString[i] !== "8" && valueString[i] !== "9") {
              i += 1;
            }
            if (i === 0) {
              elem.reference.value = valueString.charCodeAt(0);
              i += 1;
            } else {
              elem.reference.value = parseInt(valueString.slice(0, i), 8);
            }
            if (valueString.length > i) {
              const tail = elem.stack.splice(elem.index + 1);
              for (const char of valueString.slice(i)) {
                elem.stack.push({
                  type: types_1.types.CHAR,
                  value: char.charCodeAt(0)
                });
              }
              elem.stack.push(...tail);
            }
          }
        }
      }
    }
  }
});

// node_modules/ret/dist/sets-lookup.js
var require_sets_lookup = __commonJS({
  "node_modules/ret/dist/sets-lookup.js"(exports) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __setModuleDefault = exports && exports.__setModuleDefault || (Object.create ? (function(o, v) {
      Object.defineProperty(o, "default", { enumerable: true, value: v });
    }) : function(o, v) {
      o["default"] = v;
    });
    var __importStar = exports && exports.__importStar || function(mod) {
      if (mod && mod.__esModule) return mod;
      var result = {};
      if (mod != null) {
        for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
      }
      __setModuleDefault(result, mod);
      return result;
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.NOTANYCHAR = exports.WHITESPACE = exports.WORDS = exports.INTS = void 0;
    var Sets = __importStar(require_sets());
    var types_1 = require_types2();
    function setToLookup(tokens) {
      let lookup = {};
      let len = 0;
      for (const token of tokens) {
        if (token.type === types_1.types.CHAR) {
          lookup[token.value] = true;
        }
        if (token.type === types_1.types.RANGE) {
          lookup[`${token.from}-${token.to}`] = true;
        }
        len += 1;
      }
      return {
        lookup: () => Object.assign({}, lookup),
        len
      };
    }
    exports.INTS = setToLookup(Sets.ints().set);
    exports.WORDS = setToLookup(Sets.words().set);
    exports.WHITESPACE = setToLookup(Sets.whitespace().set);
    exports.NOTANYCHAR = setToLookup(Sets.anyChar().set);
  }
});

// node_modules/ret/dist/write-set-tokens.js
var require_write_set_tokens = __commonJS({
  "node_modules/ret/dist/write-set-tokens.js"(exports) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __setModuleDefault = exports && exports.__setModuleDefault || (Object.create ? (function(o, v) {
      Object.defineProperty(o, "default", { enumerable: true, value: v });
    }) : function(o, v) {
      o["default"] = v;
    });
    var __importStar = exports && exports.__importStar || function(mod) {
      if (mod && mod.__esModule) return mod;
      var result = {};
      if (mod != null) {
        for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
      }
      __setModuleDefault(result, mod);
      return result;
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.writeSetTokens = exports.setChar = void 0;
    var types_1 = require_types2();
    var sets = __importStar(require_sets_lookup());
    function setChar(charCode) {
      return charCode === 94 ? "\\^" : charCode === 92 ? "\\\\" : charCode === 93 ? "\\]" : charCode === 45 ? "\\-" : String.fromCharCode(charCode);
    }
    exports.setChar = setChar;
    function isSameSet(set, { lookup, len }) {
      if (len !== set.length) {
        return false;
      }
      const map = lookup();
      for (const elem of set) {
        if (elem.type === types_1.types.SET) {
          return false;
        }
        const key = elem.type === types_1.types.CHAR ? elem.value : `${elem.from}-${elem.to}`;
        if (map[key]) {
          map[key] = false;
        } else {
          return false;
        }
      }
      return true;
    }
    function writeSetTokens(set, isNested = false) {
      if (isSameSet(set.set, sets.INTS)) {
        return set.not ? "\\D" : "\\d";
      }
      if (isSameSet(set.set, sets.WORDS)) {
        return set.not ? "\\W" : "\\w";
      }
      if (set.not && isSameSet(set.set, sets.NOTANYCHAR)) {
        return ".";
      }
      if (isSameSet(set.set, sets.WHITESPACE)) {
        return set.not ? "\\S" : "\\s";
      }
      let tokenString = "";
      for (let i = 0; i < set.set.length; i++) {
        const subset = set.set[i];
        tokenString += writeSetToken(subset);
      }
      const contents = `${set.not ? "^" : ""}${tokenString}`;
      return isNested ? contents : `[${contents}]`;
    }
    exports.writeSetTokens = writeSetTokens;
    function writeSetToken(set) {
      if (set.type === types_1.types.CHAR) {
        return setChar(set.value);
      } else if (set.type === types_1.types.RANGE) {
        return `${setChar(set.from)}-${setChar(set.to)}`;
      }
      return writeSetTokens(set, true);
    }
  }
});

// node_modules/ret/dist/reconstruct.js
var require_reconstruct = __commonJS({
  "node_modules/ret/dist/reconstruct.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.reconstruct = void 0;
    var types_1 = require_types2();
    var write_set_tokens_1 = require_write_set_tokens();
    var reduceStack = (stack) => stack.map(exports.reconstruct).join("");
    var createAlternate = (token) => {
      if ("options" in token) {
        return token.options.map(reduceStack).join("|");
      } else if ("stack" in token) {
        return reduceStack(token.stack);
      } else {
        throw new Error(`options or stack must be Root or Group token`);
      }
    };
    exports.reconstruct = (token) => {
      switch (token.type) {
        case types_1.types.ROOT:
          return createAlternate(token);
        case types_1.types.CHAR: {
          const c = String.fromCharCode(token.value);
          return (/[[\\{}$^.|?*+()]/.test(c) ? "\\" : "") + c;
        }
        case types_1.types.POSITION:
          if (token.value === "^" || token.value === "$") {
            return token.value;
          } else {
            return `\\${token.value}`;
          }
        case types_1.types.REFERENCE:
          return `\\${token.value}`;
        case types_1.types.SET:
          return write_set_tokens_1.writeSetTokens(token);
        case types_1.types.GROUP: {
          const prefix = token.name ? `?<${token.name}>` : token.remember ? "" : token.followedBy ? "?=" : token.notFollowedBy ? "?!" : "?:";
          return `(${prefix}${createAlternate(token)})`;
        }
        case types_1.types.REPETITION: {
          const { min, max } = token;
          let endWith;
          if (min === 0 && max === 1) {
            endWith = "?";
          } else if (min === 1 && max === Infinity) {
            endWith = "+";
          } else if (min === 0 && max === Infinity) {
            endWith = "*";
          } else if (max === Infinity) {
            endWith = `{${min},}`;
          } else if (min === max) {
            endWith = `{${min}}`;
          } else {
            endWith = `{${min},${max}}`;
          }
          return `${exports.reconstruct(token.value)}${endWith}`;
        }
        case types_1.types.RANGE:
          return `${write_set_tokens_1.setChar(token.from)}-${write_set_tokens_1.setChar(token.to)}`;
        default:
          throw new Error(`Invalid token type ${token}`);
      }
    };
  }
});

// node_modules/ret/dist/index.js
var require_dist = __commonJS({
  "node_modules/ret/dist/index.js"(exports, module) {
    "use strict";
    var __createBinding = exports && exports.__createBinding || (Object.create ? (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      Object.defineProperty(o, k2, { enumerable: true, get: function() {
        return m[k];
      } });
    }) : (function(o, m, k, k2) {
      if (k2 === void 0) k2 = k;
      o[k2] = m[k];
    }));
    var __exportStar = exports && exports.__exportStar || function(m, exports2) {
      for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports2, p)) __createBinding(exports2, m, p);
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.types = void 0;
    var types_1 = require_types2();
    Object.defineProperty(exports, "types", { enumerable: true, get: function() {
      return types_1.types;
    } });
    __exportStar(require_tokenizer(), exports);
    __exportStar(require_reconstruct(), exports);
    var tokenizer_1 = require_tokenizer();
    var reconstruct_1 = require_reconstruct();
    __exportStar(require_types2(), exports);
    exports.default = tokenizer_1.tokenizer;
    module.exports = tokenizer_1.tokenizer;
    module.exports.types = types_1.types;
    module.exports.reconstruct = reconstruct_1.reconstruct;
  }
});

// node_modules/safe-regex2/index.js
var require_safe_regex2 = __commonJS({
  "node_modules/safe-regex2/index.js"(exports, module) {
    "use strict";
    var parse = require_dist();
    var types = parse.types;
    function safeRegex(re, opts) {
      if (!opts) opts = {};
      const replimit = opts.limit === void 0 ? 25 : opts.limit;
      if (isRegExp(re)) re = re.source;
      else if (typeof re !== "string") re = String(re);
      try {
        re = parse(re);
      } catch {
        return false;
      }
      let reps = 0;
      return (function walk(node, starHeight) {
        let i;
        let ok;
        let len;
        if (node.type === types.REPETITION) {
          starHeight++;
          reps++;
          if (starHeight > 1) return false;
          if (reps > replimit) return false;
        }
        if (node.options) {
          for (i = 0, len = node.options.length; i < len; i++) {
            ok = walk({ stack: node.options[i] }, starHeight);
            if (!ok) return false;
          }
        }
        const stack = node.stack || node.value?.stack;
        if (!stack) return true;
        for (i = 0; i < stack.length; i++) {
          ok = walk(stack[i], starHeight);
          if (!ok) return false;
        }
        return true;
      })(re, 0);
    }
    function isRegExp(x) {
      return {}.toString.call(x) === "[object RegExp]";
    }
    module.exports = safeRegex;
    module.exports.default = safeRegex;
    module.exports.safeRegex = safeRegex;
  }
});

// node_modules/fast-deep-equal/index.js
var require_fast_deep_equal = __commonJS({
  "node_modules/fast-deep-equal/index.js"(exports, module) {
    "use strict";
    module.exports = function equal(a, b) {
      if (a === b) return true;
      if (a && b && typeof a == "object" && typeof b == "object") {
        if (a.constructor !== b.constructor) return false;
        var length, i, keys;
        if (Array.isArray(a)) {
          length = a.length;
          if (length != b.length) return false;
          for (i = length; i-- !== 0; )
            if (!equal(a[i], b[i])) return false;
          return true;
        }
        if (a.constructor === RegExp) return a.source === b.source && a.flags === b.flags;
        if (a.valueOf !== Object.prototype.valueOf) return a.valueOf() === b.valueOf();
        if (a.toString !== Object.prototype.toString) return a.toString() === b.toString();
        keys = Object.keys(a);
        length = keys.length;
        if (length !== Object.keys(b).length) return false;
        for (i = length; i-- !== 0; )
          if (!Object.prototype.hasOwnProperty.call(b, keys[i])) return false;
        for (i = length; i-- !== 0; ) {
          var key = keys[i];
          if (!equal(a[key], b[key])) return false;
        }
        return true;
      }
      return a !== a && b !== b;
    };
  }
});

// lib/strategies/http-method.js
var require_http_method = __commonJS({
  "lib/strategies/http-method.js"(exports, module) {
    "use strict";
    module.exports = {
      name: "__fmw_internal_strategy_merged_tree_http_method__",
      storage: function() {
        const handlers = /* @__PURE__ */ new Map();
        return {
          get: (type) => {
            return handlers.get(type) || null;
          },
          set: (type, store) => {
            handlers.set(type, store);
          }
        };
      },
      /* c8 ignore next 1 */
      deriveConstraint: (req) => req.method,
      mustMatchWhenDerived: true
    };
  }
});

// lib/pretty-print.js
var require_pretty_print = __commonJS({
  "lib/pretty-print.js"(exports, module) {
    "use strict";
    var deepEqual = require_fast_deep_equal();
    var httpMethodStrategy = require_http_method();
    var treeDataSymbol = Symbol("treeData");
    function printObjectTree(obj, parentPrefix = "") {
      let tree = "";
      const keys = Object.keys(obj);
      for (let i = 0; i < keys.length; i++) {
        const key = keys[i];
        const value = obj[key];
        const isLast = i === keys.length - 1;
        const nodePrefix = isLast ? "\u2514\u2500\u2500 " : "\u251C\u2500\u2500 ";
        const childPrefix = isLast ? "    " : "\u2502   ";
        const nodeData = value[treeDataSymbol] || "";
        const prefixedNodeData = nodeData.replaceAll("\n", "\n" + parentPrefix + childPrefix);
        tree += parentPrefix + nodePrefix + key + prefixedNodeData + "\n";
        tree += printObjectTree(value, parentPrefix + childPrefix);
      }
      return tree;
    }
    function parseFunctionName(fn) {
      let fName = fn.name || "";
      fName = fName.replace("bound", "").trim();
      fName = (fName || "anonymous") + "()";
      return fName;
    }
    function parseMeta(meta) {
      if (Array.isArray(meta)) return meta.map((m) => parseMeta(m));
      if (typeof meta === "symbol") return meta.toString();
      if (typeof meta === "function") return parseFunctionName(meta);
      return meta;
    }
    function getRouteMetaData(route, options) {
      if (!options.includeMeta) return {};
      const metaDataObject = options.buildPrettyMeta(route);
      const filteredMetaData = {};
      let includeMetaKeys = options.includeMeta;
      if (!Array.isArray(includeMetaKeys)) {
        includeMetaKeys = Reflect.ownKeys(metaDataObject);
      }
      for (const metaKey of includeMetaKeys) {
        if (!Object.prototype.hasOwnProperty.call(metaDataObject, metaKey)) continue;
        const serializedKey = metaKey.toString();
        const metaValue = metaDataObject[metaKey];
        if (metaValue !== void 0 && metaValue !== null) {
          const serializedValue = JSON.stringify(parseMeta(metaValue));
          filteredMetaData[serializedKey] = serializedValue;
        }
      }
      return filteredMetaData;
    }
    function serializeMetaData(metaData) {
      let serializedMetaData = "";
      for (const [key, value] of Object.entries(metaData)) {
        serializedMetaData += `
\u2022 (${key}) ${value}`;
      }
      return serializedMetaData;
    }
    function normalizeRoute(route) {
      const constraints = { ...route.opts.constraints };
      const method = constraints[httpMethodStrategy.name];
      delete constraints[httpMethodStrategy.name];
      return { ...route, method, opts: { constraints } };
    }
    function serializeRoute(route) {
      let serializedRoute = ` (${route.method})`;
      const constraints = route.opts.constraints || {};
      if (Object.keys(constraints).length !== 0) {
        serializedRoute += " " + JSON.stringify(constraints);
      }
      serializedRoute += serializeMetaData(route.metaData);
      return serializedRoute;
    }
    function mergeSimilarRoutes(routes) {
      return routes.reduce((mergedRoutes, route) => {
        for (const nodeRoute of mergedRoutes) {
          if (deepEqual(route.opts.constraints, nodeRoute.opts.constraints) && deepEqual(route.metaData, nodeRoute.metaData)) {
            nodeRoute.method += ", " + route.method;
            return mergedRoutes;
          }
        }
        mergedRoutes.push(route);
        return mergedRoutes;
      }, []);
    }
    function serializeNode(node, prefix, options) {
      let routes = node.routes;
      if (options.method === void 0) {
        routes = routes.map(normalizeRoute);
      }
      routes = routes.map((route) => {
        route.metaData = getRouteMetaData(route, options);
        return route;
      });
      if (options.method === void 0) {
        routes = mergeSimilarRoutes(routes);
      }
      return routes.map(serializeRoute).join(`
${prefix}`);
    }
    function buildObjectTree(node, tree, prefix, options) {
      if (node.isLeafNode || options.commonPrefix !== false) {
        prefix = prefix || "(empty root node)";
        tree = tree[prefix] = {};
        if (node.isLeafNode) {
          tree[treeDataSymbol] = serializeNode(node, prefix, options);
        }
        prefix = "";
      }
      if (node.staticChildren) {
        for (const child of Object.values(node.staticChildren)) {
          buildObjectTree(child, tree, prefix + child.prefix, options);
        }
      }
      if (node.parametricChildren) {
        for (const child of Object.values(node.parametricChildren)) {
          const childPrefix = Array.from(child.nodePaths).join("|");
          buildObjectTree(child, tree, prefix + childPrefix, options);
        }
      }
      if (node.wildcardChild) {
        buildObjectTree(node.wildcardChild, tree, "*", options);
      }
    }
    function prettyPrintTree(root, options) {
      const objectTree = {};
      buildObjectTree(root, objectTree, root.prefix, options);
      return printObjectTree(objectTree);
    }
    module.exports = { prettyPrintTree };
  }
});

// lib/null-object.js
var require_null_object = __commonJS({
  "lib/null-object.js"(exports, module) {
    "use strict";
    var NullObject = function() {
    };
    NullObject.prototype = /* @__PURE__ */ Object.create(null);
    module.exports = {
      NullObject
    };
  }
});

// lib/handler-storage.js
var require_handler_storage = __commonJS({
  "lib/handler-storage.js"(exports, module) {
    "use strict";
    var { NullObject } = require_null_object();
    var httpMethodStrategy = require_http_method();
    var HandlerStorage = class {
      constructor() {
        this.unconstrainedHandler = null;
        this.constraints = [];
        this.handlers = [];
        this.constrainedHandlerStores = null;
      }
      // This is the hot path for node handler finding -- change with care!
      getMatchingHandler(derivedConstraints) {
        if (derivedConstraints === void 0) {
          return this.unconstrainedHandler;
        }
        return this._getHandlerMatchingConstraints(derivedConstraints);
      }
      addHandler(constrainer, route) {
        const params = route.params;
        const constraints = route.opts.constraints || {};
        const handlerObject = {
          params,
          constraints,
          handler: route.handler,
          store: route.store || null,
          _createParamsObject: this._compileCreateParamsObject(params)
        };
        const constraintsNames = Object.keys(constraints);
        if (constraintsNames.length === 0) {
          this.unconstrainedHandler = handlerObject;
        }
        for (const constraint of constraintsNames) {
          if (!this.constraints.includes(constraint)) {
            if (constraint === "version") {
              this.constraints.unshift(constraint);
            } else {
              this.constraints.push(constraint);
            }
          }
        }
        const isMergedTree = constraintsNames.includes(httpMethodStrategy.name);
        if (!isMergedTree && this.handlers.length >= 31) {
          throw new Error("find-my-way supports a maximum of 31 route handlers per node when there are constraints, limit reached");
        }
        this.handlers.push(handlerObject);
        this.handlers.sort((a, b) => Object.keys(a.constraints).length - Object.keys(b.constraints).length);
        if (!isMergedTree) {
          this._compileGetHandlerMatchingConstraints(constrainer, constraints);
        }
      }
      _compileCreateParamsObject(params) {
        const fnBody = [];
        fnBody.push("const fn = function _createParamsObject (paramsArray) {");
        fnBody.push("const params = new NullObject()");
        for (let i = 0; i < params.length; i++) {
          fnBody.push(`params['${params[i]}'] = paramsArray[${i}]`);
        }
        fnBody.push("return params");
        fnBody.push("}");
        fnBody.push("return fn");
        return new Function("NullObject", fnBody.join("\n"))(NullObject);
      }
      _getHandlerMatchingConstraints() {
        return null;
      }
      // Builds a store object that maps from constraint values to a bitmap of handler indexes which pass the constraint for a value
      // So for a host constraint, this might look like { "fastify.io": 0b0010, "google.ca": 0b0101 }, meaning the 3rd handler is constrainted to fastify.io, and the 2nd and 4th handlers are constrained to google.ca.
      // The store's implementation comes from the strategies provided to the Router.
      _buildConstraintStore(store, constraint) {
        for (let i = 0; i < this.handlers.length; i++) {
          const handler = this.handlers[i];
          const constraintValue = handler.constraints[constraint];
          if (constraintValue !== void 0) {
            let indexes = store.get(constraintValue) || 0;
            indexes |= 1 << i;
            store.set(constraintValue, indexes);
          }
        }
      }
      // Builds a bitmask for a given constraint that has a bit for each handler index that is 0 when that handler *is* constrained and 1 when the handler *isnt* constrainted. This is opposite to what might be obvious, but is just for convienience when doing the bitwise operations.
      _constrainedIndexBitmask(constraint) {
        let mask = 0;
        for (let i = 0; i < this.handlers.length; i++) {
          const handler = this.handlers[i];
          const constraintValue = handler.constraints[constraint];
          if (constraintValue !== void 0) {
            mask |= 1 << i;
          }
        }
        return ~mask;
      }
      // Compile a fast function to match the handlers for this node
      // The function implements a general case multi-constraint matching algorithm.
      // The general idea is this: we have a bunch of handlers, each with a potentially different set of constraints, and sometimes none at all. We're given a list of constraint values and we have to use the constraint-value-comparison strategies to see which handlers match the constraint values passed in.
      // We do this by asking each constraint store which handler indexes match the given constraint value for each store. Trickily, the handlers that a store says match are the handlers constrained by that store, but handlers that aren't constrained at all by that store could still match just fine. So, each constraint store can only describe matches for it, and it won't have any bearing on the handlers it doesn't care about. For this reason, we have to ask each stores which handlers match and track which have been matched (or not cared about) by all of them.
      // We use bitmaps to represent these lists of matches so we can use bitwise operations to implement this efficiently. Bitmaps are cheap to allocate, let us implement this masking behaviour in one CPU instruction, and are quite compact in memory. We start with a bitmap set to all 1s representing every handler that is a match candidate, and then for each constraint, see which handlers match using the store, and then mask the result by the mask of handlers that that store applies to, and bitwise AND with the candidate list. Phew.
      // We consider all this compiling function complexity to be worth it, because the naive implementation that just loops over the handlers asking which stores match is quite a bit slower.
      _compileGetHandlerMatchingConstraints(constrainer) {
        this.constrainedHandlerStores = {};
        for (const constraint of this.constraints) {
          const store = constrainer.newStoreForConstraint(constraint);
          this.constrainedHandlerStores[constraint] = store;
          this._buildConstraintStore(store, constraint);
        }
        const lines = [];
        lines.push(`
    let candidates = ${(1 << this.handlers.length) - 1}
    let mask, matches
    `);
        for (const constraint of this.constraints) {
          lines.push(`
      mask = ${this._constrainedIndexBitmask(constraint)}
      value = derivedConstraints.${constraint}
      `);
          const strategy = constrainer.strategies[constraint];
          const matchMask = strategy.mustMatchWhenDerived ? "matches" : "(matches | mask)";
          lines.push(`
      if (value === undefined) {
        candidates &= mask
      } else {
        matches = this.constrainedHandlerStores.${constraint}.get(value) || 0
        candidates &= ${matchMask}
      }
      if (candidates === 0) return null;
      `);
        }
        for (const constraint in constrainer.strategies) {
          const strategy = constrainer.strategies[constraint];
          if (strategy.mustMatchWhenDerived && !this.constraints.includes(constraint)) {
            lines.push(`if (derivedConstraints.${constraint} !== undefined) return null`);
          }
        }
        lines.push("return this.handlers[Math.floor(Math.log2(candidates))]");
        this._getHandlerMatchingConstraints = new Function("derivedConstraints", lines.join("\n"));
      }
    };
    module.exports = HandlerStorage;
  }
});

// lib/node.js
var require_node = __commonJS({
  "lib/node.js"(exports, module) {
    "use strict";
    var HandlerStorage = require_handler_storage();
    var NODE_TYPES = {
      STATIC: 0,
      PARAMETRIC: 1,
      WILDCARD: 2
    };
    var Node = class {
      constructor() {
        this.isLeafNode = false;
        this.routes = null;
        this.handlerStorage = null;
      }
      addRoute(route, constrainer) {
        if (this.routes === null) {
          this.routes = [];
        }
        if (this.handlerStorage === null) {
          this.handlerStorage = new HandlerStorage();
        }
        this.isLeafNode = true;
        this.routes.push(route);
        this.handlerStorage.addHandler(constrainer, route);
      }
    };
    var ParentNode = class extends Node {
      constructor() {
        super();
        this.staticChildren = {};
      }
      findStaticMatchingChild(path, pathIndex) {
        const staticChild = this.staticChildren[path.charAt(pathIndex)];
        if (staticChild === void 0 || !staticChild.matchPrefix(path, pathIndex)) {
          return null;
        }
        return staticChild;
      }
      getStaticChild(path, pathIndex = 0) {
        if (path.length === pathIndex) {
          return this;
        }
        const staticChild = this.findStaticMatchingChild(path, pathIndex);
        if (staticChild) {
          return staticChild.getStaticChild(path, pathIndex + staticChild.prefix.length);
        }
        return null;
      }
      createStaticChild(path) {
        if (path.length === 0) {
          return this;
        }
        let staticChild = this.staticChildren[path.charAt(0)];
        if (staticChild) {
          let i = 1;
          for (; i < staticChild.prefix.length; i++) {
            if (path.charCodeAt(i) !== staticChild.prefix.charCodeAt(i)) {
              staticChild = staticChild.split(this, i);
              break;
            }
          }
          return staticChild.createStaticChild(path.slice(i));
        }
        const label = path.charAt(0);
        this.staticChildren[label] = new StaticNode(path);
        return this.staticChildren[label];
      }
    };
    var StaticNode = class _StaticNode extends ParentNode {
      constructor(prefix) {
        super();
        this.prefix = prefix;
        this.wildcardChild = null;
        this.parametricChildren = [];
        this.kind = NODE_TYPES.STATIC;
        this._compilePrefixMatch();
      }
      getParametricChild(regex) {
        const regexpSource = regex && regex.source;
        const parametricChild = this.parametricChildren.find((child) => {
          const childRegexSource = child.regex && child.regex.source;
          return childRegexSource === regexpSource;
        });
        if (parametricChild) {
          return parametricChild;
        }
        return null;
      }
      createParametricChild(regex, staticSuffix, nodePath) {
        let parametricChild = this.getParametricChild(regex);
        if (parametricChild) {
          parametricChild.nodePaths.add(nodePath);
          return parametricChild;
        }
        parametricChild = new ParametricNode(regex, staticSuffix, nodePath);
        this.parametricChildren.push(parametricChild);
        this.parametricChildren.sort((child1, child2) => {
          if (!child1.isRegex) return 1;
          if (!child2.isRegex) return -1;
          if (child1.staticSuffix === null) return 1;
          if (child2.staticSuffix === null) return -1;
          if (child2.staticSuffix.endsWith(child1.staticSuffix)) return 1;
          if (child1.staticSuffix.endsWith(child2.staticSuffix)) return -1;
          return 0;
        });
        return parametricChild;
      }
      getWildcardChild() {
        return this.wildcardChild;
      }
      createWildcardChild() {
        this.wildcardChild = this.getWildcardChild() || new WildcardNode();
        return this.wildcardChild;
      }
      split(parentNode, length) {
        const parentPrefix = this.prefix.slice(0, length);
        const childPrefix = this.prefix.slice(length);
        this.prefix = childPrefix;
        this._compilePrefixMatch();
        const staticNode = new _StaticNode(parentPrefix);
        staticNode.staticChildren[childPrefix.charAt(0)] = this;
        parentNode.staticChildren[parentPrefix.charAt(0)] = staticNode;
        return staticNode;
      }
      getNextNode(path, pathIndex, nodeStack, paramsCount) {
        let node = this.findStaticMatchingChild(path, pathIndex);
        let parametricBrotherNodeIndex = 0;
        if (node === null) {
          if (this.parametricChildren.length === 0) {
            return this.wildcardChild;
          }
          node = this.parametricChildren[0];
          parametricBrotherNodeIndex = 1;
        }
        if (this.wildcardChild !== null) {
          nodeStack.push({
            paramsCount,
            brotherPathIndex: pathIndex,
            brotherNode: this.wildcardChild
          });
        }
        for (let i = this.parametricChildren.length - 1; i >= parametricBrotherNodeIndex; i--) {
          nodeStack.push({
            paramsCount,
            brotherPathIndex: pathIndex,
            brotherNode: this.parametricChildren[i]
          });
        }
        return node;
      }
      _compilePrefixMatch() {
        if (this.prefix.length === 1) {
          this.matchPrefix = () => true;
          return;
        }
        const lines = [];
        for (let i = 1; i < this.prefix.length; i++) {
          const charCode = this.prefix.charCodeAt(i);
          lines.push(`path.charCodeAt(i + ${i}) === ${charCode}`);
        }
        this.matchPrefix = new Function("path", "i", `return ${lines.join(" && ")}`);
      }
    };
    var ParametricNode = class extends ParentNode {
      constructor(regex, staticSuffix, nodePath) {
        super();
        this.isRegex = !!regex;
        this.regex = regex || null;
        this.staticSuffix = staticSuffix || null;
        this.kind = NODE_TYPES.PARAMETRIC;
        this.nodePaths = /* @__PURE__ */ new Set([nodePath]);
      }
      getNextNode(path, pathIndex) {
        return this.findStaticMatchingChild(path, pathIndex);
      }
    };
    var WildcardNode = class extends Node {
      constructor() {
        super();
        this.kind = NODE_TYPES.WILDCARD;
      }
      getNextNode() {
        return null;
      }
    };
    module.exports = { StaticNode, ParametricNode, WildcardNode, NODE_TYPES };
  }
});

// lib/strategies/accept-version.js
var require_accept_version = __commonJS({
  "lib/strategies/accept-version.js"(exports, module) {
    "use strict";
    var assert = require_assert_stub();
    function SemVerStore() {
      if (!(this instanceof SemVerStore)) {
        return new SemVerStore();
      }
      this.store = /* @__PURE__ */ new Map();
      this.maxMajor = 0;
      this.maxMinors = {};
      this.maxPatches = {};
    }
    SemVerStore.prototype.set = function(version, store) {
      if (typeof version !== "string") {
        throw new TypeError("Version should be a string");
      }
      let [major, minor, patch] = version.split(".", 3);
      if (isNaN(major)) {
        throw new TypeError("Major version must be a numeric value");
      }
      major = Number(major);
      minor = Number(minor) || 0;
      patch = Number(patch) || 0;
      if (major >= this.maxMajor) {
        this.maxMajor = major;
        this.store.set("x", store);
        this.store.set("*", store);
        this.store.set("x.x", store);
        this.store.set("x.x.x", store);
      }
      if (minor >= (this.maxMinors[major] || 0)) {
        this.maxMinors[major] = minor;
        this.store.set(`${major}.x`, store);
        this.store.set(`${major}.x.x`, store);
      }
      if (patch >= (this.maxPatches[`${major}.${minor}`] || 0)) {
        this.maxPatches[`${major}.${minor}`] = patch;
        this.store.set(`${major}.${minor}.x`, store);
      }
      this.store.set(`${major}.${minor}.${patch}`, store);
      return this;
    };
    SemVerStore.prototype.get = function(version) {
      return this.store.get(version);
    };
    module.exports = {
      name: "version",
      mustMatchWhenDerived: true,
      storage: SemVerStore,
      validate(value) {
        assert(typeof value === "string", "Version should be a string");
      }
    };
  }
});

// lib/strategies/accept-host.js
var require_accept_host = __commonJS({
  "lib/strategies/accept-host.js"(exports, module) {
    "use strict";
    var assert = require_assert_stub();
    function HostStorage() {
      const hosts = /* @__PURE__ */ new Map();
      const regexHosts = [];
      return {
        get: (host) => {
          const exact = hosts.get(host);
          if (exact) {
            return exact;
          }
          for (const regex of regexHosts) {
            if (regex.host.test(host)) {
              return regex.value;
            }
          }
        },
        set: (host, value) => {
          if (host instanceof RegExp) {
            regexHosts.push({ host, value });
          } else {
            hosts.set(host, value);
          }
        }
      };
    }
    module.exports = {
      name: "host",
      mustMatchWhenDerived: false,
      storage: HostStorage,
      validate(value) {
        assert(typeof value === "string" || Object.prototype.toString.call(value) === "[object RegExp]", "Host should be a string or a RegExp");
      }
    };
  }
});

// lib/constrainer.js
var require_constrainer = __commonJS({
  "lib/constrainer.js"(exports, module) {
    "use strict";
    var acceptVersionStrategy = require_accept_version();
    var acceptHostStrategy = require_accept_host();
    var assert = require_assert_stub();
    var Constrainer = class {
      constructor(customStrategies) {
        this.strategies = {
          version: acceptVersionStrategy,
          host: acceptHostStrategy
        };
        this.strategiesInUse = /* @__PURE__ */ new Set();
        this.asyncStrategiesInUse = /* @__PURE__ */ new Set();
        if (customStrategies) {
          for (const strategy of Object.values(customStrategies)) {
            this.addConstraintStrategy(strategy);
          }
        }
      }
      isStrategyUsed(strategyName) {
        return this.strategiesInUse.has(strategyName) || this.asyncStrategiesInUse.has(strategyName);
      }
      hasConstraintStrategy(strategyName) {
        const customConstraintStrategy = this.strategies[strategyName];
        if (customConstraintStrategy !== void 0) {
          return customConstraintStrategy.isCustom || this.isStrategyUsed(strategyName);
        }
        return false;
      }
      addConstraintStrategy(strategy) {
        assert(typeof strategy.name === "string" && strategy.name !== "", "strategy.name is required.");
        assert(strategy.storage && typeof strategy.storage === "function", "strategy.storage function is required.");
        assert(strategy.deriveConstraint && typeof strategy.deriveConstraint === "function", "strategy.deriveConstraint function is required.");
        if (this.strategies[strategy.name] && this.strategies[strategy.name].isCustom) {
          throw new Error(`There already exists a custom constraint with the name ${strategy.name}.`);
        }
        if (this.isStrategyUsed(strategy.name)) {
          throw new Error(`There already exists a route with ${strategy.name} constraint.`);
        }
        strategy.isCustom = true;
        strategy.isAsync = strategy.deriveConstraint.length === 3;
        this.strategies[strategy.name] = strategy;
        if (strategy.mustMatchWhenDerived) {
          this.noteUsage({ [strategy.name]: strategy });
        }
      }
      deriveConstraints(req, ctx, done) {
        const constraints = this.deriveSyncConstraints(req, ctx);
        if (done === void 0) {
          return constraints;
        }
        this.deriveAsyncConstraints(constraints, req, ctx, done);
      }
      deriveSyncConstraints(req, ctx) {
        return void 0;
      }
      // When new constraints start getting used, we need to rebuild the deriver to derive them. Do so if we see novel constraints used.
      noteUsage(constraints) {
        if (constraints) {
          const beforeSize = this.strategiesInUse.size;
          for (const key in constraints) {
            const strategy = this.strategies[key];
            if (strategy.isAsync) {
              this.asyncStrategiesInUse.add(key);
            } else {
              this.strategiesInUse.add(key);
            }
          }
          if (beforeSize !== this.strategiesInUse.size) {
            this._buildDeriveConstraints();
          }
        }
      }
      newStoreForConstraint(constraint) {
        if (!this.strategies[constraint]) {
          throw new Error(`No strategy registered for constraint key ${constraint}`);
        }
        return this.strategies[constraint].storage();
      }
      validateConstraints(constraints) {
        for (const key in constraints) {
          const value = constraints[key];
          if (typeof value === "undefined") {
            throw new Error("Can't pass an undefined constraint value, must pass null or no key at all");
          }
          const strategy = this.strategies[key];
          if (!strategy) {
            throw new Error(`No strategy registered for constraint key ${key}`);
          }
          if (strategy.validate) {
            strategy.validate(value);
          }
        }
      }
      deriveAsyncConstraints(constraints, req, ctx, done) {
        let asyncConstraintsCount = this.asyncStrategiesInUse.size;
        if (asyncConstraintsCount === 0) {
          done(null, constraints);
          return;
        }
        constraints = constraints || {};
        for (const key of this.asyncStrategiesInUse) {
          const strategy = this.strategies[key];
          strategy.deriveConstraint(req, ctx, (err, constraintValue) => {
            if (err !== null) {
              done(err);
              return;
            }
            constraints[key] = constraintValue;
            if (--asyncConstraintsCount === 0) {
              done(null, constraints);
            }
          });
        }
      }
      // Optimization: build a fast function for deriving the constraints for all the strategies at once. We inline the definitions of the version constraint and the host constraint for performance.
      // If no constraining strategies are in use (no routes constrain on host, or version, or any custom strategies) then we don't need to derive constraints for each route match, so don't do anything special, and just return undefined
      // This allows us to not allocate an object to hold constraint values if no constraints are defined.
      _buildDeriveConstraints() {
        if (this.strategiesInUse.size === 0) return;
        const lines = ["return {"];
        for (const key of this.strategiesInUse) {
          const strategy = this.strategies[key];
          if (!strategy.isCustom) {
            if (key === "version") {
              lines.push("   version: req.headers['accept-version'],");
            } else {
              lines.push("   host: req.headers.host || req.headers[':authority'],");
            }
          } else {
            lines.push(`  ${strategy.name}: this.strategies.${key}.deriveConstraint(req, ctx),`);
          }
        }
        lines.push("}");
        this.deriveSyncConstraints = new Function("req", "ctx", lines.join("\n")).bind(this);
      }
    };
    module.exports = Constrainer;
  }
});

// lib/http-methods.js
var require_http_methods = __commonJS({
  "lib/http-methods.js"(exports, module) {
    "use strict";
    var httpMethods = [
      "ACL",
      "BIND",
      "CHECKOUT",
      "CONNECT",
      "COPY",
      "DELETE",
      "GET",
      "HEAD",
      "LINK",
      "LOCK",
      "M-SEARCH",
      "MERGE",
      "MKACTIVITY",
      "MKCALENDAR",
      "MKCOL",
      "MOVE",
      "NOTIFY",
      "OPTIONS",
      "PATCH",
      "POST",
      "PROPFIND",
      "PROPPATCH",
      "PURGE",
      "PUT",
      "QUERY",
      "REBIND",
      "REPORT",
      "SEARCH",
      "SOURCE",
      "SUBSCRIBE",
      "TRACE",
      "UNBIND",
      "UNLINK",
      "UNLOCK",
      "UNSUBSCRIBE"
    ];
    module.exports = httpMethods;
  }
});

// lib/url-sanitizer.js
var require_url_sanitizer = __commonJS({
  "lib/url-sanitizer.js"(exports, module) {
    "use strict";
    function decodeComponentChar(highCharCode, lowCharCode) {
      if (highCharCode === 50) {
        if (lowCharCode === 53) return "%";
        if (lowCharCode === 51) return "#";
        if (lowCharCode === 52) return "$";
        if (lowCharCode === 54) return "&";
        if (lowCharCode === 66) return "+";
        if (lowCharCode === 98) return "+";
        if (lowCharCode === 67) return ",";
        if (lowCharCode === 99) return ",";
        if (lowCharCode === 70) return "/";
        if (lowCharCode === 102) return "/";
        return null;
      }
      if (highCharCode === 51) {
        if (lowCharCode === 65) return ":";
        if (lowCharCode === 97) return ":";
        if (lowCharCode === 66) return ";";
        if (lowCharCode === 98) return ";";
        if (lowCharCode === 68) return "=";
        if (lowCharCode === 100) return "=";
        if (lowCharCode === 70) return "?";
        if (lowCharCode === 102) return "?";
        return null;
      }
      if (highCharCode === 52 && lowCharCode === 48) {
        return "@";
      }
      return null;
    }
    function safeDecodeURI(path, useSemicolonDelimiter) {
      let shouldDecode = false;
      let shouldDecodeParam = false;
      let querystring = "";
      for (let i = 1; i < path.length; i++) {
        const charCode = path.charCodeAt(i);
        if (charCode === 37) {
          const highCharCode = path.charCodeAt(i + 1);
          const lowCharCode = path.charCodeAt(i + 2);
          if (decodeComponentChar(highCharCode, lowCharCode) === null) {
            shouldDecode = true;
          } else {
            shouldDecodeParam = true;
            if (highCharCode === 50 && lowCharCode === 53) {
              shouldDecode = true;
              path = path.slice(0, i + 1) + "25" + path.slice(i + 1);
              i += 2;
            }
            i += 2;
          }
        } else if (charCode === 63 || charCode === 35 || charCode === 59 && useSemicolonDelimiter) {
          querystring = path.slice(i + 1);
          path = path.slice(0, i);
          break;
        }
      }
      const decodedPath = shouldDecode ? decodeURI(path) : path;
      return { path: decodedPath, querystring, shouldDecodeParam };
    }
    function safeDecodeURIComponent(uriComponent) {
      const startIndex = uriComponent.indexOf("%");
      if (startIndex === -1) return uriComponent;
      let decoded = "";
      let lastIndex = startIndex;
      for (let i = startIndex; i < uriComponent.length; i++) {
        if (uriComponent.charCodeAt(i) === 37) {
          const highCharCode = uriComponent.charCodeAt(i + 1);
          const lowCharCode = uriComponent.charCodeAt(i + 2);
          const decodedChar = decodeComponentChar(highCharCode, lowCharCode);
          decoded += uriComponent.slice(lastIndex, i) + decodedChar;
          lastIndex = i + 3;
        }
      }
      return uriComponent.slice(0, startIndex) + decoded + uriComponent.slice(lastIndex);
    }
    module.exports = { safeDecodeURI, safeDecodeURIComponent };
  }
});

// index.js
var require_index = __commonJS({
  "index.js"(exports, module) {
    var assert = require_assert_stub();
    var querystring = require_lib();
    var isRegexSafe = require_safe_regex2();
    var deepEqual = require_fast_deep_equal();
    var { prettyPrintTree } = require_pretty_print();
    var { StaticNode, NODE_TYPES } = require_node();
    var Constrainer = require_constrainer();
    var httpMethods = require_http_methods();
    var httpMethodStrategy = require_http_method();
    var { safeDecodeURI, safeDecodeURIComponent } = require_url_sanitizer();
    var FULL_PATH_REGEXP = /^https?:\/\/.*?\//;
    var OPTIONAL_PARAM_REGEXP = /(\/:[^/()]*?)\?(\/?)/;
    var ESCAPE_REGEXP = /[.*+?^${}()|[\]\\]/g;
    var REMOVE_DUPLICATE_SLASHES_REGEXP = /\/\/+/g;
    if (!isRegexSafe(FULL_PATH_REGEXP)) {
      throw new Error("the FULL_PATH_REGEXP is not safe, update this module");
    }
    if (!isRegexSafe(OPTIONAL_PARAM_REGEXP)) {
      throw new Error("the OPTIONAL_PARAM_REGEXP is not safe, update this module");
    }
    if (!isRegexSafe(ESCAPE_REGEXP)) {
      throw new Error("the ESCAPE_REGEXP is not safe, update this module");
    }
    if (!isRegexSafe(REMOVE_DUPLICATE_SLASHES_REGEXP)) {
      throw new Error("the REMOVE_DUPLICATE_SLASHES_REGEXP is not safe, update this module");
    }
    function Router(opts) {
      if (!(this instanceof Router)) {
        return new Router(opts);
      }
      opts = opts || {};
      this._opts = opts;
      if (opts.defaultRoute) {
        assert(typeof opts.defaultRoute === "function", "The default route must be a function");
        this.defaultRoute = opts.defaultRoute;
      } else {
        this.defaultRoute = null;
      }
      if (opts.onBadUrl) {
        assert(typeof opts.onBadUrl === "function", "The bad url handler must be a function");
        this.onBadUrl = opts.onBadUrl;
      } else {
        this.onBadUrl = null;
      }
      if (opts.buildPrettyMeta) {
        assert(typeof opts.buildPrettyMeta === "function", "buildPrettyMeta must be a function");
        this.buildPrettyMeta = opts.buildPrettyMeta;
      } else {
        this.buildPrettyMeta = defaultBuildPrettyMeta;
      }
      if (opts.querystringParser) {
        assert(typeof opts.querystringParser === "function", "querystringParser must be a function");
        this.querystringParser = opts.querystringParser;
      } else {
        this.querystringParser = (query) => query.length === 0 ? {} : querystring.parse(query);
      }
      this.caseSensitive = opts.caseSensitive === void 0 ? true : opts.caseSensitive;
      this.ignoreTrailingSlash = opts.ignoreTrailingSlash || false;
      this.ignoreDuplicateSlashes = opts.ignoreDuplicateSlashes || false;
      this.maxParamLength = opts.maxParamLength || 100;
      this.allowUnsafeRegex = opts.allowUnsafeRegex || false;
      this.constrainer = new Constrainer(opts.constraints);
      this.useSemicolonDelimiter = opts.useSemicolonDelimiter || false;
      this.routes = [];
      this.trees = {};
    }
    Router.prototype.on = function on(method, path, opts, handler, store) {
      if (typeof opts === "function") {
        if (handler !== void 0) {
          store = handler;
        }
        handler = opts;
        opts = {};
      }
      assert(typeof path === "string", "Path should be a string");
      assert(path.length > 0, "The path could not be empty");
      assert(path[0] === "/" || path[0] === "*", "The first character of a path should be `/` or `*`");
      assert(typeof handler === "function", "Handler should be a function");
      const optionalParamMatch = path.match(OPTIONAL_PARAM_REGEXP);
      if (optionalParamMatch) {
        assert(path.length === optionalParamMatch.index + optionalParamMatch[0].length, "Optional Parameter needs to be the last parameter of the path");
        const pathFull = path.replace(OPTIONAL_PARAM_REGEXP, "$1$2");
        const pathOptional = path.replace(OPTIONAL_PARAM_REGEXP, "$2") || "/";
        this.on(method, pathFull, opts, handler, store);
        this.on(method, pathOptional, opts, handler, store);
        return;
      }
      const route = path;
      if (this.ignoreDuplicateSlashes) {
        path = removeDuplicateSlashes(path);
      }
      if (this.ignoreTrailingSlash) {
        path = trimLastSlash(path);
      }
      const methods = Array.isArray(method) ? method : [method];
      for (const method2 of methods) {
        assert(typeof method2 === "string", "Method should be a string");
        assert(httpMethods.includes(method2), `Method '${method2}' is not an http method.`);
        this._on(method2, path, opts, handler, store, route);
      }
    };
    Router.prototype._on = function _on(method, path, opts, handler, store) {
      let constraints = {};
      if (opts.constraints !== void 0) {
        assert(typeof opts.constraints === "object" && opts.constraints !== null, "Constraints should be an object");
        if (Object.keys(opts.constraints).length !== 0) {
          constraints = opts.constraints;
        }
      }
      this.constrainer.validateConstraints(constraints);
      this.constrainer.noteUsage(constraints);
      if (this.trees[method] === void 0) {
        this.trees[method] = new StaticNode("/");
      }
      let pattern = path;
      if (pattern === "*" && this.trees[method].prefix.length !== 0) {
        const currentRoot = this.trees[method];
        this.trees[method] = new StaticNode("");
        this.trees[method].staticChildren["/"] = currentRoot;
      }
      let currentNode = this.trees[method];
      let parentNodePathIndex = currentNode.prefix.length;
      const params = [];
      for (let i = 0; i <= pattern.length; i++) {
        if (pattern.charCodeAt(i) === 58 && pattern.charCodeAt(i + 1) === 58) {
          i++;
          continue;
        }
        const isParametricNode = pattern.charCodeAt(i) === 58 && pattern.charCodeAt(i + 1) !== 58;
        const isWildcardNode = pattern.charCodeAt(i) === 42;
        if (isParametricNode || isWildcardNode || i === pattern.length && i !== parentNodePathIndex) {
          let staticNodePath = pattern.slice(parentNodePathIndex, i);
          if (!this.caseSensitive) {
            staticNodePath = staticNodePath.toLowerCase();
          }
          staticNodePath = staticNodePath.replaceAll("::", ":");
          staticNodePath = staticNodePath.replaceAll("%", "%25");
          currentNode = currentNode.createStaticChild(staticNodePath);
        }
        if (isParametricNode) {
          let isRegexNode = false;
          let isParamSafe = true;
          let backtrack = "";
          const regexps = [];
          let lastParamStartIndex = i + 1;
          for (let j = lastParamStartIndex; ; j++) {
            const charCode = pattern.charCodeAt(j);
            const isRegexParam = charCode === 40;
            const isStaticPart = charCode === 45 || charCode === 46;
            const isEndOfNode = charCode === 47 || j === pattern.length;
            if (isRegexParam || isStaticPart || isEndOfNode) {
              const paramName = pattern.slice(lastParamStartIndex, j);
              params.push(paramName);
              isRegexNode = isRegexNode || isRegexParam || isStaticPart;
              if (isRegexParam) {
                const endOfRegexIndex = getClosingParenthensePosition(pattern, j);
                const regexString = pattern.slice(j, endOfRegexIndex + 1);
                if (!this.allowUnsafeRegex) {
                  assert(isRegexSafe(new RegExp(regexString)), `The regex '${regexString}' is not safe!`);
                }
                regexps.push(trimRegExpStartAndEnd(regexString));
                j = endOfRegexIndex + 1;
                isParamSafe = true;
              } else {
                regexps.push(isParamSafe ? "(.*?)" : `(${backtrack}|(?:(?!${backtrack}).)*)`);
                isParamSafe = false;
              }
              const staticPartStartIndex = j;
              for (; j < pattern.length; j++) {
                const charCode2 = pattern.charCodeAt(j);
                if (charCode2 === 47) break;
                if (charCode2 === 58) {
                  const nextCharCode = pattern.charCodeAt(j + 1);
                  if (nextCharCode === 58) j++;
                  else break;
                }
              }
              let staticPart = pattern.slice(staticPartStartIndex, j);
              if (staticPart) {
                staticPart = staticPart.replaceAll("::", ":");
                staticPart = staticPart.replaceAll("%", "%25");
                regexps.push(backtrack = escapeRegExp(staticPart));
              }
              lastParamStartIndex = j + 1;
              if (isEndOfNode || pattern.charCodeAt(j) === 47 || j === pattern.length) {
                const nodePattern = isRegexNode ? "()" + staticPart : staticPart;
                const nodePath = pattern.slice(i, j);
                pattern = pattern.slice(0, i + 1) + nodePattern + pattern.slice(j);
                i += nodePattern.length;
                const regex = isRegexNode ? new RegExp("^" + regexps.join("") + "$") : null;
                currentNode = currentNode.createParametricChild(regex, staticPart || null, nodePath);
                parentNodePathIndex = i + 1;
                break;
              }
            }
          }
        } else if (isWildcardNode) {
          params.push("*");
          currentNode = currentNode.createWildcardChild();
          parentNodePathIndex = i + 1;
          if (i !== pattern.length - 1) {
            throw new Error("Wildcard must be the last character in the route");
          }
        }
      }
      if (!this.caseSensitive) {
        pattern = pattern.toLowerCase();
      }
      if (pattern === "*") {
        pattern = "/*";
      }
      for (const existRoute of this.routes) {
        const routeConstraints = existRoute.opts.constraints || {};
        if (existRoute.method === method && existRoute.pattern === pattern && deepEqual(routeConstraints, constraints)) {
          throw new Error(`Method '${method}' already declared for route '${pattern}' with constraints '${JSON.stringify(constraints)}'`);
        }
      }
      const route = { method, path, pattern, params, opts, handler, store };
      this.routes.push(route);
      currentNode.addRoute(route, this.constrainer);
    };
    Router.prototype.hasRoute = function hasRoute(method, path, constraints) {
      const route = this.findRoute(method, path, constraints);
      return route !== null;
    };
    Router.prototype.findRoute = function findNode(method, path, constraints = {}) {
      if (this.trees[method] === void 0) {
        return null;
      }
      let pattern = path;
      let currentNode = this.trees[method];
      let parentNodePathIndex = currentNode.prefix.length;
      const params = [];
      for (let i = 0; i <= pattern.length; i++) {
        if (pattern.charCodeAt(i) === 58 && pattern.charCodeAt(i + 1) === 58) {
          i++;
          continue;
        }
        const isParametricNode = pattern.charCodeAt(i) === 58 && pattern.charCodeAt(i + 1) !== 58;
        const isWildcardNode = pattern.charCodeAt(i) === 42;
        if (isParametricNode || isWildcardNode || i === pattern.length && i !== parentNodePathIndex) {
          let staticNodePath = pattern.slice(parentNodePathIndex, i);
          if (!this.caseSensitive) {
            staticNodePath = staticNodePath.toLowerCase();
          }
          staticNodePath = staticNodePath.replaceAll("::", ":");
          staticNodePath = staticNodePath.replaceAll("%", "%25");
          currentNode = currentNode.getStaticChild(staticNodePath);
          if (currentNode === null) {
            return null;
          }
        }
        if (isParametricNode) {
          let isRegexNode = false;
          let isParamSafe = true;
          let backtrack = "";
          const regexps = [];
          let lastParamStartIndex = i + 1;
          for (let j = lastParamStartIndex; ; j++) {
            const charCode = pattern.charCodeAt(j);
            const isRegexParam = charCode === 40;
            const isStaticPart = charCode === 45 || charCode === 46;
            const isEndOfNode = charCode === 47 || j === pattern.length;
            if (isRegexParam || isStaticPart || isEndOfNode) {
              const paramName = pattern.slice(lastParamStartIndex, j);
              params.push(paramName);
              isRegexNode = isRegexNode || isRegexParam || isStaticPart;
              if (isRegexParam) {
                const endOfRegexIndex = getClosingParenthensePosition(pattern, j);
                const regexString = pattern.slice(j, endOfRegexIndex + 1);
                if (!this.allowUnsafeRegex) {
                  assert(isRegexSafe(new RegExp(regexString)), `The regex '${regexString}' is not safe!`);
                }
                regexps.push(trimRegExpStartAndEnd(regexString));
                j = endOfRegexIndex + 1;
                isParamSafe = false;
              } else {
                regexps.push(isParamSafe ? "(.*?)" : `(${backtrack}|(?:(?!${backtrack}).)*)`);
                isParamSafe = false;
              }
              const staticPartStartIndex = j;
              for (; j < pattern.length; j++) {
                const charCode2 = pattern.charCodeAt(j);
                if (charCode2 === 47) break;
                if (charCode2 === 58) {
                  const nextCharCode = pattern.charCodeAt(j + 1);
                  if (nextCharCode === 58) j++;
                  else break;
                }
              }
              let staticPart = pattern.slice(staticPartStartIndex, j);
              if (staticPart) {
                staticPart = staticPart.replaceAll("::", ":");
                staticPart = staticPart.replaceAll("%", "%25");
                regexps.push(backtrack = escapeRegExp(staticPart));
              }
              lastParamStartIndex = j + 1;
              if (isEndOfNode || pattern.charCodeAt(j) === 47 || j === pattern.length) {
                const nodePattern = isRegexNode ? "()" + staticPart : staticPart;
                const nodePath = pattern.slice(i, j);
                pattern = pattern.slice(0, i + 1) + nodePattern + pattern.slice(j);
                i += nodePattern.length;
                const regex = isRegexNode ? new RegExp("^" + regexps.join("") + "$") : null;
                currentNode = currentNode.getParametricChild(regex, staticPart || null, nodePath);
                if (currentNode === null) {
                  return null;
                }
                parentNodePathIndex = i + 1;
                break;
              }
            }
          }
        } else if (isWildcardNode) {
          params.push("*");
          currentNode = currentNode.getWildcardChild();
          parentNodePathIndex = i + 1;
          if (i !== pattern.length - 1) {
            throw new Error("Wildcard must be the last character in the route");
          }
        }
      }
      if (!this.caseSensitive) {
        pattern = pattern.toLowerCase();
      }
      for (const existRoute of this.routes) {
        const routeConstraints = existRoute.opts.constraints || {};
        if (existRoute.method === method && existRoute.pattern === pattern && deepEqual(routeConstraints, constraints)) {
          return {
            handler: existRoute.handler,
            store: existRoute.store,
            params: existRoute.params
          };
        }
      }
      return null;
    };
    Router.prototype.hasConstraintStrategy = function(strategyName) {
      return this.constrainer.hasConstraintStrategy(strategyName);
    };
    Router.prototype.addConstraintStrategy = function(constraints) {
      this.constrainer.addConstraintStrategy(constraints);
      this._rebuild(this.routes);
    };
    Router.prototype.reset = function reset() {
      this.trees = {};
      this.routes = [];
    };
    Router.prototype.off = function off(method, path, constraints) {
      assert(typeof path === "string", "Path should be a string");
      assert(path.length > 0, "The path could not be empty");
      assert(path[0] === "/" || path[0] === "*", "The first character of a path should be `/` or `*`");
      assert(
        typeof constraints === "undefined" || typeof constraints === "object" && !Array.isArray(constraints) && constraints !== null,
        "Constraints should be an object or undefined."
      );
      const optionalParamMatch = path.match(OPTIONAL_PARAM_REGEXP);
      if (optionalParamMatch) {
        assert(path.length === optionalParamMatch.index + optionalParamMatch[0].length, "Optional Parameter needs to be the last parameter of the path");
        const pathFull = path.replace(OPTIONAL_PARAM_REGEXP, "$1$2");
        const pathOptional = path.replace(OPTIONAL_PARAM_REGEXP, "$2");
        this.off(method, pathFull, constraints);
        this.off(method, pathOptional, constraints);
        return;
      }
      if (this.ignoreDuplicateSlashes) {
        path = removeDuplicateSlashes(path);
      }
      if (this.ignoreTrailingSlash) {
        path = trimLastSlash(path);
      }
      const methods = Array.isArray(method) ? method : [method];
      for (const method2 of methods) {
        this._off(method2, path, constraints);
      }
    };
    Router.prototype._off = function _off(method, path, constraints) {
      assert(typeof method === "string", "Method should be a string");
      assert(httpMethods.includes(method), `Method '${method}' is not an http method.`);
      function matcherWithoutConstraints(route) {
        return method !== route.method || path !== route.path;
      }
      function matcherWithConstraints(route) {
        return matcherWithoutConstraints(route) || !deepEqual(constraints, route.opts.constraints || {});
      }
      const predicate = constraints ? matcherWithConstraints : matcherWithoutConstraints;
      const newRoutes = this.routes.filter(predicate);
      this._rebuild(newRoutes);
    };
    Router.prototype.lookup = function lookup(req, res, ctx, done) {
      if (typeof ctx === "function") {
        done = ctx;
        ctx = void 0;
      }
      if (done === void 0) {
        const constraints = this.constrainer.deriveConstraints(req, ctx);
        const handle = this.find(req.method, req.url, constraints);
        return this.callHandler(handle, req, res, ctx);
      }
      this.constrainer.deriveConstraints(req, ctx, (err, constraints) => {
        if (err !== null) {
          done(err);
          return;
        }
        try {
          const handle = this.find(req.method, req.url, constraints);
          const result = this.callHandler(handle, req, res, ctx);
          done(null, result);
        } catch (err2) {
          done(err2);
        }
      });
    };
    Router.prototype.callHandler = function callHandler(handle, req, res, ctx) {
      if (handle === null) return this._defaultRoute(req, res, ctx);
      return ctx === void 0 ? handle.handler(req, res, handle.params, handle.store, handle.searchParams) : handle.handler.call(ctx, req, res, handle.params, handle.store, handle.searchParams);
    };
    Router.prototype.find = function find(method, path, derivedConstraints) {
      let currentNode = this.trees[method];
      if (currentNode === void 0) return null;
      if (path.charCodeAt(0) !== 47) {
        path = path.replace(FULL_PATH_REGEXP, "/");
      }
      if (this.ignoreDuplicateSlashes) {
        path = removeDuplicateSlashes(path);
      }
      let sanitizedUrl;
      let querystring2;
      let shouldDecodeParam;
      try {
        sanitizedUrl = safeDecodeURI(path, this.useSemicolonDelimiter);
        path = sanitizedUrl.path;
        querystring2 = sanitizedUrl.querystring;
        shouldDecodeParam = sanitizedUrl.shouldDecodeParam;
      } catch (error) {
        return this._onBadUrl(path);
      }
      if (this.ignoreTrailingSlash) {
        path = trimLastSlash(path);
      }
      const originPath = path;
      if (this.caseSensitive === false) {
        path = path.toLowerCase();
      }
      const maxParamLength = this.maxParamLength;
      let pathIndex = currentNode.prefix.length;
      const params = [];
      const pathLen = path.length;
      const brothersNodesStack = [];
      while (true) {
        if (pathIndex === pathLen && currentNode.isLeafNode) {
          const handle = currentNode.handlerStorage.getMatchingHandler(derivedConstraints);
          if (handle !== null) {
            return {
              handler: handle.handler,
              store: handle.store,
              params: handle._createParamsObject(params),
              searchParams: this.querystringParser(querystring2)
            };
          }
        }
        let node = currentNode.getNextNode(path, pathIndex, brothersNodesStack, params.length);
        if (node === null) {
          if (brothersNodesStack.length === 0) {
            return null;
          }
          const brotherNodeState = brothersNodesStack.pop();
          pathIndex = brotherNodeState.brotherPathIndex;
          params.splice(brotherNodeState.paramsCount);
          node = brotherNodeState.brotherNode;
        }
        currentNode = node;
        if (currentNode.kind === NODE_TYPES.STATIC) {
          pathIndex += currentNode.prefix.length;
          continue;
        }
        if (currentNode.kind === NODE_TYPES.WILDCARD) {
          let param2 = originPath.slice(pathIndex);
          if (shouldDecodeParam) {
            param2 = safeDecodeURIComponent(param2);
          }
          params.push(param2);
          pathIndex = pathLen;
          continue;
        }
        let paramEndIndex = originPath.indexOf("/", pathIndex);
        if (paramEndIndex === -1) {
          paramEndIndex = pathLen;
        }
        let param = originPath.slice(pathIndex, paramEndIndex);
        if (shouldDecodeParam) {
          param = safeDecodeURIComponent(param);
        }
        if (currentNode.isRegex) {
          const matchedParameters = currentNode.regex.exec(param);
          if (matchedParameters === null) continue;
          for (let i = 1; i < matchedParameters.length; i++) {
            const matchedParam = matchedParameters[i];
            if (matchedParam.length > maxParamLength) {
              return null;
            }
            params.push(matchedParam);
          }
        } else {
          if (param.length > maxParamLength) {
            return null;
          }
          params.push(param);
        }
        pathIndex = paramEndIndex;
      }
    };
    Router.prototype._rebuild = function(routes) {
      this.reset();
      for (const route of routes) {
        const { method, path, opts, handler, store } = route;
        this._on(method, path, opts, handler, store);
      }
    };
    Router.prototype._defaultRoute = function(req, res, ctx) {
      if (this.defaultRoute !== null) {
        return ctx === void 0 ? this.defaultRoute(req, res) : this.defaultRoute.call(ctx, req, res);
      } else {
        res.statusCode = 404;
        res.end();
      }
    };
    Router.prototype._onBadUrl = function(path) {
      if (this.onBadUrl === null) {
        return null;
      }
      const onBadUrl = this.onBadUrl;
      return {
        handler: (req, res, ctx) => onBadUrl(path, req, res),
        params: {},
        store: null
      };
    };
    Router.prototype.prettyPrint = function(options = {}) {
      const method = options.method;
      options.buildPrettyMeta = this.buildPrettyMeta.bind(this);
      let tree = null;
      if (method === void 0) {
        const { version, host, ...constraints } = this.constrainer.strategies;
        constraints[httpMethodStrategy.name] = httpMethodStrategy;
        const mergedRouter = new Router({ ...this._opts, constraints });
        const mergedRoutes = this.routes.map((route) => {
          const constraints2 = {
            ...route.opts.constraints,
            [httpMethodStrategy.name]: route.method
          };
          return { ...route, method: "MERGED", opts: { constraints: constraints2 } };
        });
        mergedRouter._rebuild(mergedRoutes);
        tree = mergedRouter.trees.MERGED;
      } else {
        tree = this.trees[method];
      }
      if (tree == null) return "(empty tree)";
      return prettyPrintTree(tree, options);
    };
    for (const i in httpMethods) {
      if (!httpMethods.hasOwnProperty(i)) continue;
      const m = httpMethods[i];
      const methodName = m.toLowerCase();
      Router.prototype[methodName] = function(path, handler, store) {
        return this.on(m, path, handler, store);
      };
    }
    Router.prototype.all = function(path, handler, store) {
      this.on(httpMethods, path, handler, store);
    };
    module.exports = Router;
    function escapeRegExp(string) {
      return string.replace(ESCAPE_REGEXP, "\\$&");
    }
    function removeDuplicateSlashes(path) {
      return path.indexOf("//") !== -1 ? path.replace(REMOVE_DUPLICATE_SLASHES_REGEXP, "/") : path;
    }
    function trimLastSlash(path) {
      if (path.length > 1 && path.charCodeAt(path.length - 1) === 47) {
        return path.slice(0, -1);
      }
      return path;
    }
    function trimRegExpStartAndEnd(regexString) {
      if (regexString.charCodeAt(1) === 94) {
        regexString = regexString.slice(0, 1) + regexString.slice(2);
      }
      if (regexString.charCodeAt(regexString.length - 2) === 36) {
        regexString = regexString.slice(0, regexString.length - 2) + regexString.slice(regexString.length - 1);
      }
      return regexString;
    }
    function getClosingParenthensePosition(path, idx) {
      let parentheses = 1;
      while (idx < path.length) {
        idx++;
        if (path.charCodeAt(idx) === 92) {
          idx++;
          continue;
        }
        if (path.charCodeAt(idx) === 41) {
          parentheses--;
        } else if (path.charCodeAt(idx) === 40) {
          parentheses++;
        }
        if (!parentheses) return idx;
      }
      throw new TypeError('Invalid regexp expression in "' + path + '"');
    }
    function defaultBuildPrettyMeta(route) {
      if (!route) return {};
      if (!route.store) return {};
      return Object.assign({}, route.store);
    }
  }
});
export default require_index();
