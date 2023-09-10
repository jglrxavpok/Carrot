using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Carrot.ComponentPropertyAttributes;

namespace Carrot {

    public class IntRange {
        public int Min;
        public int Max;

        public IntRange(int min, int max) {
            Min = min;
            Max = max;
        }
    }
    
    public class FloatRange {
        public float Min;
        public float Max;

        public FloatRange(float min, float max) {
            Min = min;
            Max = max;
        }
    }

    public class BoxedFloat {
        public float Value;

        public BoxedFloat(float value) {
            Value = value;
        }
    }

    public class BoxedInt {
        public int Value;

        public BoxedInt(int value) {
            Value = value;
        }
    }

    public class ComponentProperty {
        /**
         * Name of field
         */
        public string FieldName = null;
        
        /**
         * Name used for serialization. Defaults to field name if null
         */
        public string SerializationName = null;
        
        /**
         * Name displayed inside editor. Defaults to field name if null
         */
        public string DisplayName = null;

        /**
         * Type of the component
         */
        public string Type = null;

        /**
         * Valid if type == 'int'. Specifies the range that can be input inside the editor. Null if no limit
         */
        public IntRange IntRange = null;
        
        /**
         * Valid if type == 'float'. Specifies the range that can be input inside the editor. Null if no limit
         */
        public FloatRange FloatRange = null;

        public BoxedFloat DefaultFloatValue = null;
        public BoxedInt DefaultIntValue = null;
    };
    
    public static class Reflection {

        // https://stackoverflow.com/a/20008954
        private static Type FindTypeByName(string namespaceName, string className) {
            string fullName = namespaceName + '.' + className;
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies().Reverse()) {
                var tt = assembly.GetType(fullName);
                if (tt != null) {
                    return tt;
                }
            }

            return null;
        }

        private static T GetAttribute<T>(Dictionary<Type, Attribute> fieldAttributes) where T: Attribute {
            return (T)fieldAttributes[typeof(T)];
        }
        
        /**
         * Finds all the component properties inside the input type.
         * A component property is currently any public field.
         */
        public static ComponentProperty[] FindAllComponentProperties(string namespaceName, string className) {
            Type componentType = FindTypeByName(namespaceName, className);
            if (componentType == null) {
                return null;
            }

            ArrayList properties = new ArrayList();
            foreach(var field in componentType.GetFields()) {
                if (!field.IsPublic) {
                    continue;
                }

                ComponentProperty prop = new ComponentProperty {
                    Type = field.FieldType.FullName,
                    FieldName = field.Name,
                };
                
                var fieldAttributes = new Dictionary<Type, Attribute>();
                foreach (var fieldAttribute in field.GetCustomAttributes(true)) {
                    fieldAttributes.Add(fieldAttribute.GetType(), (Attribute)fieldAttribute);
                }

                // handle ranges
                if (field.FieldType == typeof(int)) {
                    if (fieldAttributes.ContainsKey(typeof(IntRangeAttribute))) {
                        var attribute = GetAttribute<IntRangeAttribute>(fieldAttributes);
                        prop.IntRange = new IntRange(attribute.Min, attribute.Max);
                    }
                } else if (field.FieldType == typeof(float)) {
                    if (fieldAttributes.ContainsKey(typeof(FloatRangeAttribute))) {
                        var attribute = GetAttribute<FloatRangeAttribute>(fieldAttributes);
                        prop.FloatRange = new FloatRange(attribute.Min, attribute.Max);
                    }
                }
                
                // handle serialization
                if (fieldAttributes.ContainsKey(typeof(SerializationNameAttribute))) {
                    prop.SerializationName = GetAttribute<SerializationNameAttribute>(fieldAttributes).Value;
                }
                
                // handle display
                if (fieldAttributes.ContainsKey(typeof(DisplayNameAttribute))) {
                    prop.DisplayName = GetAttribute<DisplayNameAttribute>(fieldAttributes).Value;
                }
                
                // handle default values
                if (fieldAttributes.ContainsKey(typeof(FloatDefaultAttribute))) {
                    prop.DefaultFloatValue = new BoxedFloat(GetAttribute<FloatDefaultAttribute>(fieldAttributes).Value);
                }
                if (fieldAttributes.ContainsKey(typeof(IntDefaultAttribute))) {
                    prop.DefaultIntValue = new BoxedInt(GetAttribute<IntDefaultAttribute>(fieldAttributes).Value);
                }

                properties.Add(prop);
            }

            ComponentProperty[] output = new ComponentProperty[properties.Count];
            properties.CopyTo(output);
            return output;
        }

        /**
         * Returns true iif the given class has the [InternalComponent] attribute
         */
        public static bool IsInternalComponent(string namespaceName, string className) {
            Type componentType = FindTypeByName(namespaceName, className);
            if (componentType == null) {
                return false;
            }

            foreach (var classAttribute in componentType.GetCustomAttributes(true)) {
                if (classAttribute.GetType() == typeof(InternalComponentAttribute)) {
                    return true;
                }
            }

            return false;
        }
    }
}
