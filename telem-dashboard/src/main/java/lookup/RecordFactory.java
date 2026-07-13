package lookup;
import java.lang.reflect.*;
import java.util.*;
import java.util.function.Function;

//Used by TelemetryLookup to instantiate its records from CSV rows.
public class RecordFactory {
    private static final Map<Class<?>, Constructor<?>> constructorCache = new HashMap<>();
    private static final Map<Class<?>, RecordComponent[]> componentCache = new HashMap<>();

    @SuppressWarnings("unchecked")
    public static <T> T createRecord(Class<T> recordClass,
                                     Function<String, String> getter,
                                     Map<String, Object> injectedValues) {
        try {
            RecordComponent[] components = componentCache.computeIfAbsent(
                recordClass, Class::getRecordComponents
            );
            Constructor<?> ctor = constructorCache.computeIfAbsent(
                recordClass,
                cls -> {
                    try {
                        Class<?>[] paramTypes = Arrays.stream(cls.getRecordComponents())
                                                      .map(RecordComponent::getType)
                                                      .toArray(Class<?>[]::new);
                        return cls.getDeclaredConstructor(paramTypes);
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            );

            Object[] args = new Object[components.length];
            for (int i = 0; i < components.length; i++) {
                String name = components[i].getName();
                Class<?> type = components[i].getType();

                // Prefer injected values
                if (injectedValues.containsKey(name)) {
                    args[i] = injectedValues.get(name);
                } else {
                    String raw = getter.apply(name);
                    if (raw == null) {
                        throw new IllegalArgumentException(
                            "Missing value for field " + name + " in " + recordClass.getSimpleName()
                        );
                    }

                    if (type == int.class || type == Integer.class) {
                        args[i] = Integer.parseInt(raw);
                    } else if (type == String.class) {
                        args[i] = raw;
                    } else if (type == boolean.class || type == Boolean.class) {
                        String lowerRaw = raw.trim().toLowerCase();
                        if (lowerRaw.equals("true") || lowerRaw.equals("1")) {
                            args[i] = true;
                        } else if (lowerRaw.equals("false") || lowerRaw.equals("0")) {
                            args[i] = false;
                        } else {
                            throw new IllegalArgumentException(
                                "Invalid boolean value '" + raw + "' for field " + name + ". Expected 'true', 'false', '0', or '1'."
                            );
                        }
                    } else {
                        throw new IllegalArgumentException(
                            "Unsupported field type: " + type.getName()
                        );
                    }
                }
            }
            return (T) ctor.newInstance(args);
        } catch (Exception e) {
            throw new RuntimeException("Failed to create " + recordClass.getSimpleName(), e);
        }
    }
}
