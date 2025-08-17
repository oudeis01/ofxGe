#include "MinimalBuiltinChecker.h"

// ================================================================================
// GLSL BUILT-IN DATA TYPES REGISTRY
// ================================================================================
// Keep this section organized by type category for easy maintenance

// Boolean types
const std::unordered_set<std::string> MinimalBuiltinChecker::boolean_types = {
    "bool", "bvec2", "bvec3", "bvec4"
};

// Integer types
const std::unordered_set<std::string> MinimalBuiltinChecker::integer_types = {
    "int", "ivec2", "ivec3", "ivec4"
};

// Unsigned integer types
const std::unordered_set<std::string> MinimalBuiltinChecker::unsigned_integer_types = {
    "uint", "uvec2", "uvec3", "uvec4"
};

// Floating point types
const std::unordered_set<std::string> MinimalBuiltinChecker::float_types = {
    "float", "vec2", "vec3", "vec4"
};

// Double precision types
const std::unordered_set<std::string> MinimalBuiltinChecker::double_types = {
    "double", "dvec2", "dvec3", "dvec4"
};

// Matrix types
const std::unordered_set<std::string> MinimalBuiltinChecker::matrix_types = {
    "mat2", "mat3", "mat4",
    "mat2x2", "mat2x3", "mat2x4",
    "mat3x2", "mat3x3", "mat3x4",
    "mat4x2", "mat4x3", "mat4x4"
};

// Double precision matrix types
const std::unordered_set<std::string> MinimalBuiltinChecker::double_matrix_types = {
    "dmat2", "dmat3", "dmat4",
    "dmat2x2", "dmat2x3", "dmat2x4",
    "dmat3x2", "dmat3x3", "dmat3x4",
    "dmat4x2", "dmat4x3", "dmat4x4"
};

// ================================================================================
// GLSL BUILT-IN FUNCTIONS REGISTRY  
// ================================================================================
// Functions organized by GLSL specification categories

// 1. Angle and Trigonometry Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::angle_trigonometry_functions = {
    "radians", "degrees",
    "sin", "cos", "tan",
    "asin", "acos", "atan"
};

// 2. Exponential Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::exponential_functions = {
    "pow", "exp", "log", "exp2", "log2", "sqrt", "inversesqrt"
};

// 3. Common Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::common_functions = {
    "abs", "sign", "floor", "trunc", "round", "roundEven", "ceil", "fract",
    "mod", "modf", "min", "max", "clamp", "mix", "step", "smoothstep"
};

// 4. Geometric Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::geometric_functions = {
    "length", "distance", "dot", "cross", "normalize", 
    "faceforward", "reflect", "refract"
};

// 5. Matrix Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::matrix_functions = {
    "matrixCompMult"
};

// 6. Vector Relational Functions
const std::unordered_set<std::string> MinimalBuiltinChecker::vector_relational_functions = {
    "lessThan", "lessThanEqual", "greaterThan", "greaterThanEqual",
    "equal", "notEqual", "any", "all", "not"
};

// ================================================================================
// PUBLIC INTERFACE METHODS
// ================================================================================

bool MinimalBuiltinChecker::isBuiltinDataType(const std::string& type_name) {
    return checkInDataTypeSets(type_name);
}

bool MinimalBuiltinChecker::isBuiltinFunction(const std::string& function_name) {
    return checkInFunctionSets(function_name);
}

bool MinimalBuiltinChecker::isBuiltin(const std::string& name) {
    return isBuiltinDataType(name) || isBuiltinFunction(name);
}

std::vector<std::string> MinimalBuiltinChecker::getAllBuiltinDataTypes() {
    std::vector<std::string> all_types;
    
    // Collect from all data type sets
    const std::vector<const std::unordered_set<std::string>*> type_sets = {
        &boolean_types, &integer_types, &unsigned_integer_types,
        &float_types, &double_types, &matrix_types, &double_matrix_types
    };
    
    for (const auto* type_set : type_sets) {
        for (const auto& type : *type_set) {
            all_types.push_back(type);
        }
    }
    
    return all_types;
}

std::vector<std::string> MinimalBuiltinChecker::getAllBuiltinFunctions() {
    std::vector<std::string> all_functions;
    
    // Collect from all function sets
    const std::vector<const std::unordered_set<std::string>*> function_sets = {
        &angle_trigonometry_functions, &exponential_functions, &common_functions,
        &geometric_functions, &matrix_functions, &vector_relational_functions
    };
    
    for (const auto* function_set : function_sets) {
        for (const auto& function : *function_set) {
            all_functions.push_back(function);
        }
    }
    
    return all_functions;
}

size_t MinimalBuiltinChecker::getBuiltinCount() {
    return getAllBuiltinDataTypes().size() + getAllBuiltinFunctions().size();
}

// ================================================================================
// HELPER METHODS
// ================================================================================

bool MinimalBuiltinChecker::checkInDataTypeSets(const std::string& name) {
    return boolean_types.find(name) != boolean_types.end() ||
           integer_types.find(name) != integer_types.end() ||
           unsigned_integer_types.find(name) != unsigned_integer_types.end() ||
           float_types.find(name) != float_types.end() ||
           double_types.find(name) != double_types.end() ||
           matrix_types.find(name) != matrix_types.end() ||
           double_matrix_types.find(name) != double_matrix_types.end();
}

bool MinimalBuiltinChecker::checkInFunctionSets(const std::string& name) {
    return angle_trigonometry_functions.find(name) != angle_trigonometry_functions.end() ||
           exponential_functions.find(name) != exponential_functions.end() ||
           common_functions.find(name) != common_functions.end() ||
           geometric_functions.find(name) != geometric_functions.end() ||
           matrix_functions.find(name) != matrix_functions.end() ||
           vector_relational_functions.find(name) != vector_relational_functions.end();
}