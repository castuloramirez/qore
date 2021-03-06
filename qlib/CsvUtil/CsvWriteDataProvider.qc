# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvWriteDataProvider class definition

/*  CsvWriteDataProvider.qc Copyright 2012 - 2020 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

# assume local var scope, do not use "$" for vars, members, and method calls
%new-style
%strict-args
%require-types
%enable-all-warnings

#! the CsvUtil namespace. All classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
#! Provides a data provider for reading CSV files
public class CsvWriteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "CsvWriteDataProvider",
            "supports_create": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the path to the CSV data; mutually exclusive with \"stream\"",
            },
            "stream": <DataProviderOptionInfo>{
                "type": AbstractDataProviderType::get(new Type("OutputStream")),
                "desc": "an output stream for CSV data; mutually exclusive with \"path\"",
            },
            "opts": <DataProviderOptionInfo>{
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "CSV generation options",
            },
        };
    }

    private {
        #! the writer object
        AbstractCsvWriter writer;
    }

    #! Creates the CsvWriteDataProvider with the output path and optionally an option hash
    /** @param path the output file to write
        @param opts a hash of options; see @ref csvwriter_options for more information

        @throw CSVFILEWRITER-ERROR option error
    */
    constructor(string path, *hash<auto> opts) {
        writer = new CsvFileWriter(path, opts);
    }

    #! Creates the CsvWriteDataProvider with the output data stream where data will be writeen and an option hash
    /** @param stream the output stream for writing the data
        @param opts a hash of optional options; see @ref csvwriter_options for more information

        @throw CSVSTREAMWRITER-ERROR option error
    */
    constructor(OutputStream stream, *hash<auto> opts) {
        writer = new CsvWriter(stream, opts);
    }

    #! Returns the object name
    string getName() {
        return writer instanceof CsvFileWriter
            ? (writer.getFileName() ?? sprintf("stream %s", self.uniqueHash()))
            : sprintf("stream %s", self.uniqueHash());
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"path\" options; use either one or the other");
            }
            writer = new CsvFileWriter(copts.path, copts.opts);
            return;
        }
        if (copts.stream) {
            writer = new CsvWriter(copts.stream, copts.opts);
            return;
        }
        # create a dummy stream output object
        writer = new CsvWriter(new StringOutputStream(), copts.opts);
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return writer.getRecordType();
    }

    #! Creates the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        writer.writeLine(rec);
        return rec;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
} # CsvUtil namespace
