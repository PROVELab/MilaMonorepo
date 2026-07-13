// IntConstUtils.java
package util;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.*;

//For mapping integers that represent enums onto the name for that enum.
//Example, if have a class like:
//  public class MyConstants {
//      public static final int chat = 1;
//      public static final int swag = 2;
//  }
//Then can call IntConstUtils.nameFromInt(MyConstants.class, 1) will return the corresponding string "chat"

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