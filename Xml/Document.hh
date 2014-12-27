<?hh // strict
/**
 * @copyright   2010-2015, The Titon Project
 * @license     http://opensource.org/licenses/bsd-license.php
 * @link        http://titon.io
 */

namespace Titon\Type\Xml;

use Titon\Common\Exception\MissingFileException;
use Titon\Utility\Col;
use \SimpleXMLElement;

type XmlMap = Map<string, mixed>;

/**
 * The Document class provides helper methods for XML parsing and building as well as static methods
 * for generating Element trees based on maps and raw XML files.
 *
 * @package Titon\Type
 */
class Document {

    /**
     * Box a value by type casting it from a string to a scalar.
     *
     * @param string $value
     * @return mixed
     */
    public static function box(string $value): mixed {
        if (is_numeric($value)) {
            if (strpos($value, '.') !== false) {
                return (float) $value;

            } else {
                return (int) $value;
            }

        } else if ($value === 'true' || $value === 'false') {
            return ($value === 'true');

        } else if ($value === 'null') {
            return null;
        }

        return (string) $value;
    }

    /**
     * Unbox values by type casting to a string equivalent.
     *
     * @param mixed $value
     * @return string
     */
    public static function unbox(mixed $value): string {
        if (is_bool($value)) {
            return $value ? 'true' : 'false';

        } else if ($value === null) {
            return 'null';
        }

        return (string) $value;
    }

    /**
     * Format an element or attribute name for converting to camel case.
     * If the element starts with a number, prefix it with an underscore.
     *
     * @return string
     */
    public static function formatName(string $name): string {
        $name = preg_replace('/[^a-z0-9:\.\-_]+/i', '', $name);
        $firstChar = mb_substr($name, 0, 1);

        if (is_numeric($firstChar) || $firstChar === '-' || $firstChar === '.') {
            $name = '_' . $name;
        }

        return $name;
    }

    /**
     * Depending on the type of data, return an Element tree.
     *
     * @param mixed $data
     * @param string $root
     * @return \Titon\Type\Xml\Element
     */
    public static function from(mixed $data, string $root = 'root'): Element {
        if ($data instanceof Map) {
            return static::fromMap($root, $data);

        } else if ($data instanceof Vector) {
            return static::fromVector($root, 'item', $data);

        } else if (is_array($data)) {
            return static::fromMap($root, Col::toMap($data));
        }

        return static::fromString((string) $data);
    }

    /**
     * Load an XML file from the file system and transform it into an Element tree.
     *
     * @param string $path
     * @return \Titon\Type\Xml\Element
     * @throws \Titon\Common\Exception\MissingFileException
     */
    public static function fromFile(string $path): Element {
        if (file_exists($path)) {
            return static::fromString(file_get_contents($path));
        }

        throw new MissingFileException(sprintf('File %s does not exist', $path));
    }

    /**
     * Transform a structure consisting of maps and vectors into an Element tree.
     *
     * @param string $root
     * @param \Titon\Type\XmlMap $map
     * @return \Titon\Type\Xml\Element
     */
    public static function fromMap(string $root, XmlMap $map): Element {
        $root = new Element($root);

        static::_addAttributes($root, $map);

        foreach ($map as $key => $value) {
            static::_createElement($root, $key, $value);
        }

        return $root;
    }

    /**
     * Transform a string representation of an XML document into an Element tree.
     *
     * @param string $string
     * @return \Titon\Type\Xml\Element
     */
    public static function fromString(string $string): Element {
        return static::_convertSimpleXml(simplexml_load_string($string));
    }

    /**
     * Transform a list of items into an Element tree.
     *
     * @param string $root
     * @param string $item
     * @param Vector<Tv> $list
     * @return \Titon\Type\Xml\Element
     */
    public static function fromVector<Tv>(string $root, string $item, Vector<Tv> $list): Element {
        $root = new Element($root);

        static::_createElement($root, $item, $list);

        return $root;
    }

    /**
     * Add attributes to an element if the special `@attributes` map exists.
     *
     * @param \Titon\Type\Xml\Element $element
     * @param \Titon\Type\XmlMap $map
     */
    protected static function _addAttributes(Element $element, XmlMap $map): void {
        if ($map->contains('@attributes')) {
            $attributes = $map->get('@attributes');

            if ($attributes instanceof Map) {
                $element->setAttributes($attributes);
            }

            $map->remove('@attributes');
        }
    }

    /**
     * Create an element and set the value/children depending on the data structure.
     *
     *  - If a map is provided, it is either an element with a value, or a list of children with different names.
     *  - If a vector is provided, it is a list of elements with the same name.
     *  - If a scalar value is provided, it is a literal element with a value.
     *
     * @param \Titon\Type\Xml\Element $parent
     * @param string $key
     * @param mixed $value
     */
    protected static function _createElement(Element $parent, string $key, mixed $value): void {

        // One of two things:
        // An element that contains other children
        // An element that has attributes or a value
        if ($value instanceof Map) {

            // An element with a value
            if ($value->contains('@value')) {
                $child = new Element($key);
                $child->setValue($value['@value'], (bool) $value->get('@cdata'));

                // Add attributes to the child
                static::_addAttributes($child, $value);

                $parent->addChild($child);

            // Multiple elements as children
            } else {
                $parent->addChild( static::fromMap($key, $value) );
            }

        // Multiple children with the same element name
        } else if ($value instanceof Vector) {
            foreach ($value as $item) {
                static::_createElement($parent, $key, $item);
            }

        // Child element with a value
        } else {
            $parent->addChild( (new Element($key))->setValue($value) );
        }
    }

    /**
     * Converts a SimpleXmlElement tree into an Element tree.
     *
     * @param \SimpleXMLElement $xml
     * @return \Titon\Type\Xml\Element
     */
    protected static function _convertSimpleXml(SimpleXMLElement $xml): Element {
        $element = new Element($xml->getName());

        // Set attributes
        if ($attributes = $xml->attributes()) {
            foreach ($attributes as $key => $value) {
                $element->setAttribute((string) $key, (string) $value);
            }
        }

        // Set namespaces
        if ($namespaces = $xml->getNamespaces()) {
            foreach ($namespaces as $key => $value) {
                $element->setNamespace((string) $key, (string) $value);
            }
        }

        // Set value
        $element->setValue(trim((string) $xml));

        // Add children
        foreach ($xml->children() as $child) {
            $element->addChild( static::_convertSimpleXml($child) );
        }

        return $element;
    }

}