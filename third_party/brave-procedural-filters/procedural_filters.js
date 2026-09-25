// Bundled from brave-core's procedural_filters.ts at
// a46f4e864ae33b38e02bee1d72f95364d4677290 by scripts/sync_procedural_filters.py.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.
var omawebProceduralFilters = (() => {
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

  // third_party/brave-procedural-filters/src/procedural_filters.ts
  var procedural_filters_exports = {};
  __export(procedural_filters_exports, {
    applyCompiledSelector: () => applyCompiledSelector,
    compileAndApplyProceduralSelector: () => compileAndApplyProceduralSelector,
    compileProceduralSelector: () => compileProceduralSelector
  });
  var W = window;
  var _asHTMLElement = (node) => {
    return node instanceof HTMLElement ? node : null;
  };
  var _compileRegEx = (regexText) => {
    const regexParts = regexText.split("/");
    const regexPattern = regexParts[1];
    const regexArgs = regexParts[2];
    const regex = new W.RegExp(regexPattern, regexArgs);
    return regex;
  };
  var _testMatches = (test, value, exact = false) => {
    if (test[0] === "/") {
      return value.match(_compileRegEx(test)) !== null;
    }
    if (test === "") {
      return value.trim() === "";
    }
    if (exact) {
      return value === test;
    }
    return value.includes(test);
  };
  var _extractKeyFromStr = (text) => {
    const quotedTerminator = '"=';
    const unquotedTerminator = "=";
    const isQuotedCase = text[0] === '"';
    const [terminator, needlePosition] = isQuotedCase ? [quotedTerminator, 1] : [unquotedTerminator, 0];
    const indexOfTerminator = text.indexOf(terminator, needlePosition);
    if (indexOfTerminator === -1) {
      let key = text;
      if (isQuotedCase) {
        if (!text.endsWith('"')) {
          throw new Error(`Quoted value '${text}' does not terminate with quote`);
        }
        key = text.slice(1, text.length - 1);
      }
      return [key, void 0];
    }
    const testCaseStr = text.slice(needlePosition, indexOfTerminator);
    const finalNeedlePosition = indexOfTerminator + terminator.length;
    return [testCaseStr, finalNeedlePosition];
  };
  var _extractValueMatchRuleFromStr = (text, uriEncode = false, needlePosition = 0) => {
    const testCaseStr = _extractValueFromStr(text, uriEncode, needlePosition);
    const testCaseFunc = _testMatches.bind(void 0, testCaseStr);
    return testCaseFunc;
  };
  var _extractValueFromStr = (text, uriEncode = false, needlePosition = 0) => {
    const isQuotedCase = text[needlePosition] === '"';
    let endIndex;
    if (isQuotedCase) {
      if (text.at(-1) !== '"') {
        throw new Error(
          `Unable to parse value rule from ${text}. Value rule starts with " but doesn't end with "`
        );
      }
      needlePosition += 1;
      endIndex = text.length - 1;
    } else {
      endIndex = text.length;
    }
    let testCaseStr = text.slice(needlePosition, endIndex);
    if (uriEncode) {
      testCaseStr = testCaseStr.replace(
        /\P{ASCII}/gu,
        (c) => encodeURIComponent(c)
      );
    }
    return testCaseStr;
  };
  var _parseKeyValueMatchRules = (arg) => {
    const [key, needlePos] = _extractKeyFromStr(arg);
    const keyMatchRule = (arg2) => _testMatches(key, arg2, true);
    let valueMatchRule;
    if (needlePos !== void 0) {
      const value = _extractValueFromStr(arg, false, needlePos);
      valueMatchRule = (arg2) => _testMatches(value, arg2, true);
    }
    return [keyMatchRule, valueMatchRule];
  };
  var _parseCSSInstruction = (arg) => {
    const rs = arg.split(":");
    if (rs.length !== 2) {
      throw Error(`Unexpected format for a CSS rule: ${arg}`);
    }
    return [rs[0].trim(), rs[1].trim()];
  };
  var _allOtherSiblings = (element) => {
    if (!element.parentNode) {
      return [];
    }
    const siblings = Array.from(element.parentNode.children);
    const otherHTMLElements = [];
    for (const sib of siblings) {
      if (sib === element) {
        continue;
      }
      const siblingHTMLElement = _asHTMLElement(sib);
      if (siblingHTMLElement !== null) {
        otherHTMLElements.push(siblingHTMLElement);
      }
    }
    return otherHTMLElements;
  };
  var _nextSiblingElement = (element) => {
    if (!element.parentNode) {
      return null;
    }
    const siblings = W.Array.from(element.parentNode.children);
    const indexOfElm = siblings.indexOf(element);
    const nextSibling = siblings[indexOfElm + 1];
    if (nextSibling === void 0) {
      return null;
    }
    return _asHTMLElement(nextSibling);
  };
  var _allChildren = (element) => {
    return W.Array.from(element.children).map((e) => _asHTMLElement(e)).filter((e) => e !== null);
  };
  var _allChildrenRecursive = (element) => {
    return W.Array.from(element.querySelectorAll(":scope *")).map((e) => _asHTMLElement(e)).filter((e) => e !== null);
  };
  var _stripCssOperator = (operator, selector) => {
    if (selector[0] !== operator) {
      throw new Error(
        `Expected to find ${operator} in initial position of "${selector}`
      );
    }
    return selector.replace(operator, "").trimStart();
  };
  var operatorCssSelector = (selector, element) => {
    const trimmedSelector = selector.trimStart();
    if (trimmedSelector.startsWith("+")) {
      const subOperator = _stripCssOperator("+", trimmedSelector);
      if (subOperator === null) {
        return [];
      }
      const nextSibNode = _nextSiblingElement(element);
      if (nextSibNode === null) {
        return [];
      }
      return nextSibNode.matches(subOperator) ? [nextSibNode] : [];
    } else if (trimmedSelector.startsWith("~")) {
      const subOperator = _stripCssOperator("~", trimmedSelector);
      if (subOperator === null) {
        return [];
      }
      const allSiblingNodes = _allOtherSiblings(element);
      return allSiblingNodes.filter((x) => x.matches(subOperator));
    } else if (trimmedSelector.startsWith(">")) {
      const subOperator = _stripCssOperator(">", trimmedSelector);
      if (subOperator === null) {
        return [];
      }
      const allChildNodes = _allChildren(element);
      return allChildNodes.filter((x) => x.matches(subOperator));
    } else if (selector.startsWith(" ")) {
      return Array.from(element.querySelectorAll(":scope " + trimmedSelector));
    }
    if (element.matches(selector)) {
      return [element];
    }
    return [];
  };
  var _hasPlainSelectorCase = (selector, element) => {
    return element.matches(selector) ? [element] : [];
  };
  var _hasProceduralSelectorCase = (selector, element) => {
    const shouldBeGreedy = selector[0]?.type !== "css-selector";
    const initElements = shouldBeGreedy ? _allChildrenRecursive(element) : [element];
    const matches = compileAndApplyProceduralSelector(selector, initElements);
    return matches.length === 0 ? [] : [element];
  };
  var operatorHas = (instruction, element) => {
    if (W.Array.isArray(instruction)) {
      return _hasProceduralSelectorCase(instruction, element);
    } else {
      return _hasPlainSelectorCase(instruction, element);
    }
  };
  var operatorHasText = (instruction, element) => {
    const text = element.innerText;
    const valueTest = _extractValueMatchRuleFromStr(instruction);
    return valueTest(text) ? [element] : [];
  };
  var _notPlainSelectorCase = (selector, element) => {
    return element.matches(selector) ? [] : [element];
  };
  var _notProceduralSelectorCase = (selector, element) => {
    const matches = compileAndApplyProceduralSelector(selector, [element]);
    return matches.length === 0 ? [element] : [];
  };
  var operatorNot = (instruction, element) => {
    if (Array.isArray(instruction)) {
      return _notProceduralSelectorCase(instruction, element);
    } else {
      return _notPlainSelectorCase(instruction, element);
    }
  };
  var operatorMatchesProperty = (instruction, element) => {
    const [keyTest, valueTest] = _parseKeyValueMatchRules(instruction);
    for (const [propName, propValue] of Object.entries(element)) {
      if (!keyTest(propName)) {
        continue;
      }
      if (valueTest !== void 0 && !valueTest(propValue)) {
        continue;
      }
      return [element];
    }
    return [];
  };
  var operatorMinTextLength = (instruction, element) => {
    const minLength = +instruction;
    if (minLength === W.NaN) {
      throw new Error(`min-text-length: Invalid arg, ${instruction}`);
    }
    return element.innerText.trim().length >= minLength ? [element] : [];
  };
  var operatorMatchesAttr = (instruction, element) => {
    const [keyTest, valueTest] = _parseKeyValueMatchRules(instruction);
    for (const attrName of element.getAttributeNames()) {
      if (!keyTest(attrName)) {
        continue;
      }
      const attrValue = element.getAttribute(attrName);
      if (attrValue === null || valueTest !== void 0 && !valueTest(attrValue)) {
        continue;
      }
      return [element];
    }
    return [];
  };
  var operatorMatchesCSS = (beforeOrAfter, cssInstruction, element) => {
    const [cssKey, expectedVal] = _parseCSSInstruction(cssInstruction);
    const elmStyle = W.getComputedStyle(element, beforeOrAfter);
    const styleValue = elmStyle.getPropertyValue(cssKey);
    if (styleValue === void 0) {
      return [];
    }
    let matched;
    if (expectedVal.startsWith("/") && expectedVal.endsWith("/")) {
      matched = styleValue.match(_compileRegEx(expectedVal)) !== null;
    } else {
      matched = expectedVal === styleValue;
    }
    return matched ? [element] : [];
  };
  var operatorMatchesMedia = (instruction, element) => {
    return W.matchMedia(instruction).matches ? [element] : [];
  };
  var operatorMatchesPath = (instruction, element) => {
    const pathAndQuery = W.location.pathname + W.location.search;
    const matchRule = _extractValueMatchRuleFromStr(instruction, true);
    return matchRule(pathAndQuery) ? [element] : [];
  };
  var _upwardIntCase = (intNeedle, element) => {
    if (intNeedle < 1 || intNeedle >= 256) {
      throw new Error(`upward: invalid arg, ${intNeedle}`);
    }
    let currentElement = element;
    while (currentElement !== null && intNeedle > 0) {
      currentElement = currentElement.parentNode;
      intNeedle -= 1;
    }
    if (currentElement === null) {
      return [];
    } else {
      const htmlElement = _asHTMLElement(currentElement);
      return htmlElement === null ? [] : [htmlElement];
    }
  };
  var _upwardProceduralSelectorCase = (selector, element) => {
    const childFilter = compileProceduralSelector(selector);
    let needle = element;
    while (needle !== null) {
      const currentElement = _asHTMLElement(needle);
      if (currentElement === null) {
        break;
      }
      const matches = applyCompiledSelector(childFilter, [currentElement]);
      if (matches.length !== 0) {
        return [currentElement];
      }
      needle = currentElement.parentNode;
    }
    return [];
  };
  var _upwardPlainSelectorCase = (selector, element) => {
    let needle = element;
    while (needle !== null) {
      const currentElement = _asHTMLElement(needle);
      if (currentElement === null) {
        break;
      }
      if (currentElement.matches(selector)) {
        return [currentElement];
      }
      needle = currentElement.parentNode;
    }
    return [];
  };
  var operatorUpward = (instruction, element) => {
    if (W.Number.isInteger(+instruction)) {
      return _upwardIntCase(+instruction, element);
    } else if (W.Array.isArray(instruction)) {
      return _upwardProceduralSelectorCase(instruction, element);
    } else {
      return _upwardPlainSelectorCase(instruction, element);
    }
  };
  var operatorXPath = (instruction, element) => {
    const result = W.document.evaluate(
      instruction,
      element,
      null,
      W.XPathResult.UNORDERED_NODE_ITERATOR_TYPE,
      null
    );
    const matches = [];
    let currentNode;
    while (currentNode = result.iterateNext()) {
      const currentElement = _asHTMLElement(currentNode);
      if (currentElement !== null) {
        matches.push(currentElement);
      }
    }
    return matches;
  };
  var ruleTypeToFuncMap = {
    "contains": operatorHasText,
    "css-selector": operatorCssSelector,
    "has": operatorHas,
    "has-text": operatorHasText,
    "matches-attr": operatorMatchesAttr,
    "matches-css": operatorMatchesCSS.bind(void 0, null),
    "matches-css-after": operatorMatchesCSS.bind(void 0, "::after"),
    "matches-css-before": operatorMatchesCSS.bind(void 0, "::before"),
    "matches-media": operatorMatchesMedia,
    "matches-path": operatorMatchesPath,
    "matches-property": operatorMatchesProperty,
    "min-text-length": operatorMinTextLength,
    "not": operatorNot,
    "upward": operatorUpward,
    "xpath": operatorXPath
  };
  var compileProceduralSelector = (operators) => {
    const outputOperatorList = [];
    for (const operator of operators) {
      const anOperatorFunc = ruleTypeToFuncMap[operator.type];
      const args = [operator.arg];
      if (anOperatorFunc === void 0) {
        throw new Error(
          `Not sure what to do with operator of type ${operator.type}`
        );
      }
      outputOperatorList.push({
        type: operator.type,
        func: anOperatorFunc.bind(void 0, ...args),
        args
      });
    }
    return outputOperatorList;
  };
  var fastPathOperatorTypes = ["matches-media", "matches-path"];
  var _determineInitNodesAndIndex = (selector, initNodes) => {
    let nodesToConsider = [];
    let index = 0;
    const firstOperator = selector[0];
    const firstOperatorType = firstOperator.type;
    const firstArg = firstOperator.args[0];
    if (initNodes !== void 0) {
      nodesToConsider = W.Array.from(initNodes);
    } else if (firstOperatorType === "css-selector") {
      const selector2 = firstArg;
      nodesToConsider = W.Array.from(W.document.querySelectorAll(selector2));
      index += 1;
    } else if (firstOperatorType === "xpath") {
      const xpath = firstArg;
      nodesToConsider = operatorXPath(xpath, W.document.documentElement);
      index += 1;
    } else {
      const allNodes = W.Array.from(W.document.all);
      nodesToConsider = allNodes.filter(_asHTMLElement);
    }
    return [index, nodesToConsider];
  };
  var applyCompiledSelector = (selector, initNodes) => {
    const initState = _determineInitNodesAndIndex(selector, initNodes);
    let [index, nodesToConsider] = initState;
    const numOperators = selector.length;
    for (index; nodesToConsider.length > 0 && index < numOperators; ++index) {
      const operator = selector[index];
      const operatorFunc = operator.func;
      const operatorType = operator.type;
      if (fastPathOperatorTypes.includes(operatorType)) {
        const firstNode = nodesToConsider[0];
        if (operatorFunc(firstNode).length === 0) {
          nodesToConsider = [];
        }
        continue;
      }
      let newNodesToConsider = [];
      for (const aNode of nodesToConsider) {
        const result = operatorFunc(aNode);
        newNodesToConsider = newNodesToConsider.concat(result);
      }
      nodesToConsider = newNodesToConsider;
    }
    return nodesToConsider;
  };
  var compileAndApplyProceduralSelector = (selector, initElements) => {
    const compiled = compileProceduralSelector(selector);
    return applyCompiledSelector(compiled, initElements);
  };
  return __toCommonJS(procedural_filters_exports);
})();
