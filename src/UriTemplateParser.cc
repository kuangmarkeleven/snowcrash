//
//  UriTemplateParser.cc
//  snowcrash
//
//  Created by Carl Griffiths on 24/02/14.
//  Copyright (c) 2014 Apiary Inc. All rights reserved.
//
#include <iomanip>
#include "UriTemplateParser.h"
#include "RegexMatch.h"

using namespace snowcrash;

static bool HasMismatchedCurlyBrackets(const URITemplate& uriTemplate) {
    int openCount = 0;
    int closeCount = 0;

    for (unsigned int i = 0; i < uriTemplate.length(); i++) {
        if (uriTemplate[i] == '{') openCount++;
        if (uriTemplate[i] == '}') closeCount++;
    }
    return openCount != closeCount;
}

static bool HasNestedCurlyBrackets(const URITemplate& uriTemplate) {
    char lastBracket = ' ';
    bool result = false;

    for (unsigned int i = 0; i < uriTemplate.length(); i++) {
        if (uriTemplate[i] == '{') {
            if (lastBracket == '{') {
                result = true;
                break;
            }
            lastBracket = '{';
        }
        if (uriTemplate[i] == '}') {
            if (lastBracket == '}') {
                result = true;
                break;
            }
            lastBracket = '}';
        }
    }
    return result;
}

static bool PathContainsSquareBrackets(const URITemplate& uriTemplate) {
    return (uriTemplate.find('[') != std::string::npos || uriTemplate.find(']') != std::string::npos);
}

static Expressions GetUriTemplateExpressions(const URITemplate& uriTemplate) {
    Expressions expressions;
    size_t expressionStartPos = 0;
    size_t expressionEndPos = 0;

    while (expressionStartPos != std::string::npos && expressionEndPos != std::string::npos && expressionStartPos < uriTemplate.length()) {
        expressionStartPos = uriTemplate.find("{", expressionStartPos);
        expressionEndPos = uriTemplate.find("}", expressionStartPos);
        if (expressionStartPos != std::string::npos && expressionEndPos > expressionStartPos) {
            expressions.push_back(uriTemplate.substr(expressionStartPos + 1, (expressionEndPos - expressionStartPos) - 1));
        }
        expressionStartPos++;
    }
    return expressions;
}

static ClassifiedExpression ClassifyExpression(const Expression& expression) {

    VariableExpression variableExpression(expression);

    if (variableExpression.IsExpressionType()) {
        return variableExpression;
    }

    QueryStringExpression queryStringExpression(expression);
    if (queryStringExpression.IsExpressionType()) {
        return queryStringExpression;
    }

    FragmentExpression fragmentExpression(expression);
    if (fragmentExpression.IsExpressionType()) {
        return fragmentExpression;
    }

    ReservedExpansionExpression reservedExpansionExpression(expression);
    if (reservedExpansionExpression.IsExpressionType()) {
        return reservedExpansionExpression;
    }

    LabelExpansionExpression labelExpansionExpression(expression);
    if (labelExpansionExpression.IsExpressionType()) {
        return labelExpansionExpression;
    }

    PathSegmentExpansionExpression pathSegmentExpansionExpression(expression);
    if (pathSegmentExpansionExpression.IsExpressionType()) {
        return pathSegmentExpansionExpression;
    }

    PathStyleParameterExpansionExpression pathStyleParameterExpansionExpression(expression);
    if (pathStyleParameterExpansionExpression.IsExpressionType()) {
        return pathSegmentExpansionExpression;
    }

    FormStyleQueryContinuationExpression formStyleQueryContinuationExpression(expression);
    if (formStyleQueryContinuationExpression.IsExpressionType()) {
        return formStyleQueryContinuationExpression;
    }

    UndefinedExpression undefinedExpression(expression);

    return undefinedExpression;
}

void URITemplateParser::parse(const URITemplate& uri, const mdp::CharactersRangeSet& sourceBlock, ParsedURITemplate& result)
{
    CaptureGroups groups;
    Expressions expressions;
    size_t gSize = 5;

    if (uri.empty()) return;

    if (RegexCapture(uri, URI_REGEX, groups, gSize)) {
        result.scheme = groups[1];
        result.host = groups[3];
        result.path = groups[4];

        if (HasMismatchedCurlyBrackets(result.path)) {
            result.report.warnings.push_back(Warning("the URI template contains mismatched expression brackets", URIWarning, MismatchedCurlyBracketsWarningUriTemplateWarningSubCode, sourceBlock));
            return;
        }

        if (HasNestedCurlyBrackets(result.path)) {
            result.report.warnings.push_back(Warning("the URI template contains nested expression brackets", URIWarning, NestedCurlyBracketsWarningUriTemplateWarningSubCode, sourceBlock));
            return;
        }

        if (PathContainsSquareBrackets(result.path)) {
            result.report.warnings.push_back(Warning("the URI template contains square brackets", URIWarning, SquareBracketWarningUriTemplateWarningSubCode, sourceBlock));
        }

        expressions = GetUriTemplateExpressions(result.path);

        ExpressionIterator currentExpression = expressions.begin();

        while (currentExpression != expressions.end()) {

            ClassifiedExpression classifiedExpression = ClassifyExpression(*currentExpression);

            if (classifiedExpression.IsSupportedExpressionType()) {
                bool hasIllegalCharacters = false;

                if (classifiedExpression.ContainsSpaces()) {
                    std::stringstream ss;
                    ss << "URI template '" << classifiedExpression.innerExpression << "' contains spaces";
                    result.report.warnings.push_back(Warning(ss.str(), URIWarning, ContainsSpacesWarningUriTemplateWarningSubCode, sourceBlock));
                    hasIllegalCharacters = true;
                }

                if (classifiedExpression.ContainsHyphens()) {
                    std::stringstream ss;
                    ss << "URI template '" << classifiedExpression.innerExpression << "' contains hyphens";
                    result.report.warnings.push_back(Warning(ss.str(), URIWarning, ContainsHyphensWarningUriTemplateWarningSubCode, sourceBlock));
                    hasIllegalCharacters = true;
                }

                if (classifiedExpression.ContainsAssignment()) {
                    std::stringstream ss;
                    ss << "URI template '" << classifiedExpression.innerExpression << "' contains assignment";
                    result.report.warnings.push_back(Warning(ss.str(), URIWarning, ContainsAssignmentWarningUriTemplateWarningSubCode, sourceBlock));
                    hasIllegalCharacters = true;
                }

                if (!hasIllegalCharacters) {
                    if (classifiedExpression.IsInvalidExpressionName()) {
                        std::stringstream ss;
                        ss << "URI template '" << classifiedExpression.innerExpression << "' contains invalid characters";
                        result.report.warnings.push_back(Warning(ss.str(), URIWarning, InvalidCharactersWarningUriTemplateWarningSubCode, sourceBlock));
                    }
                }
            }
            else{
                result.report.warnings.push_back(Warning(classifiedExpression.unsupportedWarningText, URIWarning, UnsupportedExpressionWarningUriTemplateWarningSubCode, sourceBlock));
            }
            currentExpression++;
        }
    }
    else{
        result.report.warnings.push_back(Warning("failed to parse URI Template", URIWarning, NoUriTemplateWarningSubCode));
    }

}
