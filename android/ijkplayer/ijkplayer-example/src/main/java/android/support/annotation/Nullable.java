package android.support.annotation;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.LOCAL_VARIABLE;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import java.lang.annotation.Documented;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
/**
 * Denotes that a parameter, field or method return value can be null.
 * <b>Note</b>: this is the default assumption for most Java APIs and the
 * default assumption made by most static code checking tools, so usually you
 * don't need to use this annotation; its primary use is to override a default
 * wider annotation like {@link NonNullByDefault}.
 * <p/>
 * When decorating a method call parameter, this denotes the parameter can
 * legitimately be null and the method will gracefully deal with it. Typically
 * used on optional parameters.
 * <p/>
 * When decorating a method, this denotes the method might legitimately return
 * null.
 * <p/>
 * This is a marker annotation and it has no specific attributes.
 */
@Documented
@Retention(RetentionPolicy.SOURCE)
@Target({METHOD, PARAMETER, LOCAL_VARIABLE, FIELD})
public @interface Nullable {
}