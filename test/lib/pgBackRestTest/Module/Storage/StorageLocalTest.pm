####################################################################################################################################
# StorageLocalTest.pm - Tests for Storage::Local module
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageLocalTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);

use pgBackRest::Config::Config;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Local;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule - common objects and variables used by all tests.
####################################################################################################################################
sub initModule
{
    my $self = shift;

    # Local path
    $self->{strPathLocal} = $self->testPath() . '/local';

    # Create the dynamic rule
    my $fnRule = sub
    {
        my $strRule = shift;
        my $strFile = shift;
        my $xData = shift;

        if ($strRule eq '<fn-rule-1>')
        {
            return "fn-rule-1/${xData}" . (defined($strFile) ? "/${strFile}" : '');
        }
        else
        {
            return 'fn-rule-2/' . (defined($strFile) ? "${strFile}/${strFile}" : 'no-file');
        }
    };

    # Create the rule hash
    my $hRule =
    {
        '<static-rule>' => 'static-rule-path',
        '<fn-rule-1>' =>
        {
            fnRule => $fnRule,
            xData => 'test',
        },
        '<fn-rule-2>' =>
        {
            fnRule => $fnRule,
        },
    };

    # Create local storage
    $self->{oStorageLocal} = new pgBackRest::Storage::Local(
        $self->pathLocal(), new pgBackRest::Storage::Posix::Driver(), {hRule => $hRule, bAllowTemp => false});

    # Remote path
    $self->{strPathRemote} = $self->testPath() . '/remote';

    # Create the repo path so the remote won't complain that it's missing
    mkdir($self->pathRemote())
        or confess &log(ERROR, "unable to create repo directory '" . $self->pathRemote() . qw{'});

    # Remove repo path now that the remote is created
    rmdir($self->{strPathRemote})
        or confess &log(ERROR, "unable to remove repo directory '" . $self->pathRemote() . qw{'});

    # Create remote storage
    $self->{oStorageRemote} = new pgBackRest::Storage::Local(
        $self->pathRemote(), new pgBackRest::Storage::Posix::Driver(), {hRule => $hRule});
}

####################################################################################################################################
# initTest - initialization before each test
####################################################################################################################################
sub initTest
{
    my $self = shift;

    executeTest(
        'ssh ' . $self->backrestUser() . '\@' . $self->host() . ' mkdir -m 700 ' . $self->pathRemote(), {bSuppressStdErr => true});

    executeTest('mkdir -m 700 ' . $self->pathLocal());
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Define test file
    my $strFile = 'file.txt';
    my $strFileCopy = 'file.txt.copy';
    # my $strFileHash = 'bbbcf2c59433f68f22376cd2439d6cd309378df6';
    my $strFileContent = 'TESTDATA';
    my $iFileSize = length($strFileContent);

    #---------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("pathGet()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->pathGet('<static-rule>/test', {bTemp => true})},
            ERROR_ASSERT, "temp file not supported for storage '" . $self->storageLocal()->pathBase() . "'");
        $self->testException(
            sub {$self->storageRemote()->pathGet('<static-rule>', {bTemp => true})},
            ERROR_ASSERT, 'file part must be defined when temp file specified');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageRemote()->pathGet('/file', {bTemp => true})}, "/file.tmp", 'absolute path temp');
        $self->testResult(sub {$self->storageRemote()->pathGet('/file')}, "/file", 'absolute path file');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('file')}, $self->storageLocal()->pathBase() . '/file', 'relative path');
        $self->testResult(
            sub {$self->storageRemote()->pathGet('file', {bTemp => true})},
                $self->storageRemote()->pathBase() . '/file.tmp', 'relative path temp');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->pathGet('<static-rule/file')}, ERROR_ASSERT, "found < but not > in '<static-rule/file'");

        $self->testException(
            sub {$self->storageLocal()->pathGet('<bogus-rule>')}, ERROR_ASSERT, "storage rule '<bogus-rule>' does not exist");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<static-rule>/file')},
            $self->storageLocal()->pathBase() . '/static-rule-path/file', 'static rule file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<static-rule>')},
            $self->storageLocal()->pathBase() . '/static-rule-path', 'static rule path');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-1>/file')},
            $self->storageLocal()->pathBase() . '/fn-rule-1/test/file', 'function rule 1 file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-2>/file')},
            $self->storageLocal()->pathBase() . '/fn-rule-2/file/file', 'function rule 2 file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-1>')},
            $self->storageLocal()->pathBase() . '/fn-rule-1/test', 'function rule 1 path');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-2>')},
            $self->storageLocal()->pathBase() . '/fn-rule-2/no-file', 'function rule 2 no file');
    }

    ################################################################################################################################
    if ($self->begin('openWrite()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oFileIo = $self->testResult(sub {$self->storageLocal()->openWrite($strFile)}, '[object]', 'open write');

        $self->testResult(sub {$oFileIo->write(\$strFileContent, length($strFileContent))}, $iFileSize, "write $iFileSize bytes");
        $self->testResult(sub {$oFileIo->close()}, true, 'close');
    }

    ################################################################################################################################
    if ($self->begin('put()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile))}, 0, 'put empty');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($strFile)}, 0, 'put empty (all defaults)');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile), $strFileContent)}, $iFileSize, 'put');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile), \$strFileContent)}, $iFileSize,
            'put reference');
    }

    ################################################################################################################################
    if ($self->begin('openRead()'))
    {
        my $tContent;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->openRead($strFile, {bIgnoreMissing => true})}, undef, 'ignore missing');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->openRead($strFile)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo touch ' . $self->pathLocal() . "/${strFile} && sudo chmod 700 " . $self->pathLocal() . "/${strFile}");

        $self->testException(
            sub {$self->storageLocal()->openRead($strFile)}, ERROR_FILE_OPEN,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': Permission denied");

        executeTest('sudo rm ' . $self->pathLocal() . "/${strFile}");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($self->storageLocal()->openWrite($strFile), $strFileContent);

        my $oFileIo = $self->testResult(sub {$self->storageLocal()->openRead($strFile)}, '[object]', 'open read');

        $self->testResult(sub {$oFileIo->read(\$tContent, $iFileSize)}, $iFileSize, "read $iFileSize bytes");
        $self->testResult($tContent, $strFileContent, '    check read');

        #---------------------------------------------------------------------------------------------------------------------------
        $oFileIo = $self->testResult(
            sub {$self->storageLocal()->openRead($strFile, {rhyFilter => [{strClass => STORAGE_FILTER_SHA}]})}, '[object]',
            'open read + checksum');

        undef($tContent);
        $self->testResult(sub {$oFileIo->read(\$tContent, $iFileSize)}, $iFileSize, "read $iFileSize bytes");
        $self->testResult(sub {$oFileIo->close()}, true, 'close');
        $self->testResult($tContent, $strFileContent, '    check read');
        $self->testResult($oFileIo->result(STORAGE_FILTER_SHA), sha1_hex($strFileContent), '    check hash');
    }

    ################################################################################################################################
    if ($self->begin('get()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->get($self->storageLocal()->openRead($strFile, {bIgnoreMissing => true}))}, undef,
            'get missing');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile);
        $self->testResult(sub {${$self->storageLocal()->get($strFile)}}, undef, 'get empty');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile, $strFileContent);
        $self->testResult(sub {${$self->storageLocal()->get($strFile)}}, $strFileContent, 'get');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {${$self->storageLocal()->get($self->storageLocal()->openRead($strFile))}}, $strFileContent, 'get from io');
    }

    ################################################################################################################################
    if ($self->begin('hashSize()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($strFile, $strFileContent)}, 8, 'put');

        $self->testResult(
            sub {$self->storageLocal()->hashSize($strFile)},
            qw{(} . sha1_hex($strFileContent) . ', ' . $iFileSize . qw{)}, '    check hash/size');
    }

    ################################################################################################################################
    if ($self->begin('copy()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");
        $self->testResult(
            sub {$self->storageLocal()->exists($strFileCopy)}, false, '   destination does not exist');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->copy(
                $self->storageLocal()->openRead($strFile, {bIgnoreMissing => true}),
                $self->storageLocal()->openWrite($strFileCopy))},
            false, 'missing source io');
        $self->testResult(
            sub {$self->storageLocal()->exists($strFileCopy)}, false, '   destination does not exist');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile, $strFileContent);

        $self->testResult(sub {$self->storageLocal()->copy($strFile, $strFileCopy)}, true, 'copy filename->filename');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->remove($strFileCopy);

        $self->testResult(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, true, 'copy io->filename');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->remove($strFileCopy);

        $self->testResult(
            sub {$self->storageLocal()->copy(
                $self->storageLocal()->openRead($strFile), $self->storageLocal()->openWrite($strFileCopy))},
            true, 'copy io->io');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub host {return '127.0.0.1'}
sub pathLocal {return shift->{strPathLocal}};
sub pathRemote {return shift->{strPathRemote}};
sub protocolLocal {return shift->{oProtocolLocal}};
sub protocolRemote {return shift->{oProtocolRemote}};
sub storageLocal {return shift->{oStorageLocal}};
sub storageRemote {return shift->{oStorageRemote}};

1;
