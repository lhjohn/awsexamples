#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
//#include <awsdoc/s3/s3_examples.h>
// #include <awsdoc/s3/s3_examples.h>

#define UNUSED(x) (void)(x)

// using namespace Aws;

// snippet-start:[s3.cpp.put_object_async.mutex_vars]
// A mutex is a synchronization primitive that can be used to protect shared
// data from being simultaneously accessed by multiple threads.
std::mutex upload_mutex;

// A condition_variable is a synchronization primitive that can be used to
// block a thread, or multiple threads at the same time, until another
// thread both modifies a shared variable (the condition) and
// notifies the condition_variable.
std::condition_variable upload_variable;
// snippet-end:[s3.cpp.put_object_async.mutex_vars]
// snippet-start:[s3.cpp.put_object_async_finished.code]
void PutObjectAsyncFinished(const Aws::S3::S3Client* s3Client,
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    if (outcome.IsSuccess()) {
        std::cout << "Success: PutObjectAsyncFinished: Finished uploading '"
            << context->GetUUID() << "'." << std::endl;
    }
    else {
        std::cout << "Error: PutObjectAsyncFinished: " <<
            outcome.GetError().GetMessage() << std::endl;
    }

    UNUSED(s3Client);
    UNUSED(request);
    UNUSED(context);
    // Unblock the thread that is waiting for this function to complete.
    upload_variable.notify_one();
}


// snippet-start:[s3.cpp.put_object_async.code]
bool PutObjectAsync(const Aws::S3::S3Client& s3Client,
    const Aws::String& bucketName,
    const Aws::String& objectName,
    const Aws::String& region)
{
    // Verify that the file exists.
    struct stat buffer;

    if (stat(objectName.c_str(), &buffer) == -1)
    {
        std::cout << "Error: PutObjectAsync: File '" <<
            objectName << "' does not exist." << std::endl;

        return false;
    }

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucketName);
    request.SetKey(objectName);

    const char *key = "SHA256";
    const char *value = "AnotherWay";
    // Aws::S
    request.AddMetadata(key, value);
    
    const std::shared_ptr<Aws::IOStream> input_data =
        Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
            objectName.c_str(),
            std::ios_base::in | std::ios_base::binary);

    request.SetBody(input_data);

    // Create and configure the context for the asynchronous put object request.
    std::shared_ptr<Aws::Client::AsyncCallerContext> context =
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("PutObjectAllocationTag");
    context->SetUUID(objectName);

    // Make the asynchronous put object call. Queue the request into a
    // thread executor and call the PutObjectAsyncFinished function when the
    // operation has finished.
    s3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    UNUSED(region);
    return true;
}

int main()
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {

        // TODO: Change bucket_name to the name of a bucket in your account.
        const Aws::String bucket_name = "result-store-demo";
        //TODO: Create a file called "my-file.txt" in the local folder where your executables are built to.
        const Aws::String object_name = "/Users/henrikjohn/Git/s3example/my_project_build/my-file.zip";
        //TODO: Set to the AWS Region in which the bucket was created.
        const Aws::String region = "eu-west-1";

        // A unique_lock is a general-purpose mutex ownership wrapper allowing
        // deferred locking, time-constrained attempts at locking, recursive
        // locking, transfer of lock ownership, and use with
        // condition variables.
        std::unique_lock<std::mutex> lock(upload_mutex);

        // Create and configure the Amazon S3 client.
        // This client must be declared here, as this client must exist
        // until the put object operation finishes.
        Aws::Client::ClientConfiguration config;

        if (!region.empty())
        {
            config.region = region;
        }

        Aws::S3::S3Client s3_client(config);

        auto outcome = s3_client.ListBuckets();
        if (outcome.IsSuccess()) {
            std::cout << "Found " << outcome.GetResult().GetBuckets().size() << " buckets\n";
            for (auto&& b : outcome.GetResult().GetBuckets()) {
                std::cout << b.GetName() << std::endl;
            }
        }
        else {
            std::cout << "Failed with error: " << outcome.GetError() << std::endl;
        }


        if (PutObjectAsync(s3_client, bucket_name, object_name, region)) {

            std::cout << "main: Waiting for file upload attempt..." <<
                std::endl << std::endl;

            // While the put object operation attempt is in progress,
            // you can perform other tasks.
            // This example simply blocks until the put object operation
            // attempt finishes.
            upload_variable.wait(lock);

            std::cout << std::endl << "main: File upload attempt completed."
                << std::endl;
        }
        else
        {
            return 1;
        }
    }
    Aws::ShutdownAPI(options);

    return 0;
}
