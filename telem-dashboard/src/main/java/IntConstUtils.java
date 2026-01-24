// IntConstUtils.java
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.*;

public final class IntConstUtils {
    private IntConstUtils() {}

    public static Map<Integer,String> buildIntConstantMap(Class<?> clazz) {
        Map<Integer,String> map = new HashMap<>();
        for (Field f : clazz.getDeclaredFields()) {
            int mods = f.getModifiers();
            if (Modifier.isStatic(mods) && Modifier.isFinal(mods) && f.getType() == int.class) {
                try {
                    int val = f.getInt(null);
                    map.put(val, f.getName());
                } catch (IllegalAccessException ignored) {}
            }
        }
        return map;
    }

    /** Store lookup so we only reflect on first call*/
    private static final Map<Class<?>, Map<Integer,String>> cache = new HashMap<>();

    public static Optional<String> nameFromInt(Class<?> clazz, int value) {
        Map<Integer,String> map = cache.computeIfAbsent(clazz, IntConstUtils::buildIntConstantMap);
        return Optional.ofNullable(map.get(value));
    }

    public static String flagsFromInt(Class<?> clazz, int value) {
    Map<Integer, String> map = cache.computeIfAbsent(clazz, IntConstUtils::buildIntConstantMap);

    List<String> matches = new ArrayList<>();
    for (Map.Entry<Integer, String> entry : map.entrySet()) {
        int flag = entry.getKey();
        if ((value & flag) != 0) { // flag bit is set
            matches.add(entry.getValue());
        }
    }

    if (matches.isEmpty()) {
        return "no flags set";
    }
    return String.join(", ", matches);
}
}