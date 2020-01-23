// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <iostream>
#include <string>

#include "firebase/auth.h"
#include "firebase/auth/user.h"
#include "firebase/firestore.h"
#include "firebase/util.h"

/*
 * A collection of code snippets for the Cloud Firestore C++ SDK. These snippets
 * were modelled after the existing Cloud Firestore guide, which can be found
 * here: https://firebase.google.com/docs/firestore.
 *
 * Note that not all of the Firestore API has been implemented yet, so some
 * snippets are incomplete/missing.
 */

namespace snippets {

// https://firebase.google.com/docs/firestore/quickstart#add_data
void QuickstartAddData(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;

  // Cloud Firestore stores data in Documents, which are stored in Collections.
  // Cloud Firestore creates collections and documents implicitly the first time
  // you add data to the document. You do not need to explicitly create
  // collections or documents.

  // Add a new document with a generated ID
  Future<DocumentReference> user_ref =
      db->Collection("users").Add({{"first", FieldValue::FromString("Ada")},
                                   {"last", FieldValue::FromString("Lovelace")},
                                   {"born", FieldValue::FromInteger(1815)}});

  user_ref.OnCompletion([](const Future<DocumentReference>& future) {
    if (future.error() == Error::Ok) {
      std::cout << "DocumentSnapshot added with ID: " << future.result()->id()
                << '\n';
    } else {
      std::cout << "Error adding document: " << future.error_message() << '\n';
    }
  });

  // Now add another document to the users collection. Notice that this document
  // includes a key-value pair (middle name) that does not appear in the first
  // document. Documents in a collection can contain different sets of
  // information.

  db->Collection("users")
      .Add({{"first", FieldValue::FromString("Alan")},
            {"middle", FieldValue::FromString("Mathison")},
            {"last", FieldValue::FromString("Turing")},
            {"born", FieldValue::FromInteger(1912)}})
      .OnCompletion([](const Future<DocumentReference>& future) {
        if (future.error() == Error::Ok) {
          std::cout << "DocumentSnapshot added with ID: "
                    << future.result()->id() << '\n';
        } else {
          std::cout << "Error adding document: " << future.error_message()
                    << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/quickstart#read_data
void QuickstartReadData(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::QuerySnapshot;

  // To quickly verify that you've added data to Cloud Firestore, use the data
  // viewer in the Firebase console.
  //
  // You can also use the "Get" method to retrieve the entire collection.

  Future<QuerySnapshot> users = db->Collection("users").Get();
  users.OnCompletion([](const Future<QuerySnapshot>& future) {
    if (future.error() == Error::Ok) {
      for (const DocumentSnapshot& document : future.result()->documents()) {
        std::cout << document << '\n';
      }
    } else {
      std::cout << "Error getting documents: " << future.error_message()
                << '\n';
    }
  });
}

// https://firebase.google.com/docs/firestore/manage-data/add-data#set_a_document
void AddDataSetDocument(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::SetOptions;

  // To create or overwrite a single document, use the Set() method:
  db->Collection("cities")
      .Document("LA")
      .Set({{"name", FieldValue::FromString("Los Angeles")},
            {"state", FieldValue::FromString("CA")},
            {"country", FieldValue::FromString("USA")}})
      .OnCompletion([](const Future<void>& future) {
        if (future.error() == Error::Ok) {
          std::cout << "DocumentSnapshot successfully written!\n";
        } else {
          std::cout << "Error writing document: " << future.error_message()
                    << '\n';
        }
      });

  // If the document does not exist, it will be created. If the document does
  // exist, its contents will be overwritten with the newly provided data,
  // unless you specify that the data should be merged into the existing
  // document, as follows:
  db->Collection("cities").Document("BJ").Set(
      {{"capital", FieldValue::FromBoolean(true)}}, SetOptions::Merge());
}

// https://firebase.google.com/docs/firestore/manage-data/add-data#data_types
void AddDataDataTypes(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::Timestamp;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::MapFieldValue;

  // Cloud Firestore lets you write a variety of data types inside a document,
  // including strings, booleans, numbers, dates, null, and nested arrays and
  // objects. Cloud Firestore always stores numbers as doubles, regardless of
  // what type of number you use in your code.
  MapFieldValue doc_data{
      {"stringExample", FieldValue::FromString("Hello world!")},
      {"booleanExample", FieldValue::FromBoolean(true)},
      {"numberExample", FieldValue::FromDouble(3.14159265)},
      {"dateExample", FieldValue::FromTimestamp(Timestamp::Now())},
      {"arrayExample", FieldValue::FromArray({FieldValue::FromInteger(1),
                                              FieldValue::FromInteger(2),
                                              FieldValue::FromInteger(3)})},
      {"nullExample", FieldValue::Null()},
      {"objectExample",
       FieldValue::FromMap(
           {{"a", FieldValue::FromInteger(5)},
            {"b", FieldValue::FromMap(
                      {{"nested", FieldValue::FromString("foo")}})}})},
  };

  db->Collection("data").Document("one").Set(doc_data).OnCompletion(
      [](const Future<void>& future) {
        if (future.error() == Error::Ok) {
          std::cout << "DocumentSnapshot successfully written!\n";
        } else {
          std::cout << "Error writing document: " << future.error_message()
                    << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/manage-data/add-data#add_a_document
void AddDataAddDocument(firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentReference;

  // When you use Set() to create a document, you must specify an ID for the
  // document to create. For example:
  db->Collection("cities").Document("SF").Set({/*some data*/});

  // But sometimes there isn't a meaningful ID for the document, and it's more
  // convenient to let Cloud Firestore auto-generate an ID for you. You can do
  // this by calling Add():
  db->Collection("cities").Add({/*some data*/});

  // In some cases, it can be useful to create a document reference with an
  // auto-generated ID, then use the reference later. For this use case, you can
  // call Document():
  DocumentReference new_city_ref = db->Collection("cities").Document();
  // Behind the scenes, Add(...) and Document().Set(...) are completely
  // equivalent, so you can use whichever is more convenient.
}

// https://firebase.google.com/docs/firestore/manage-data/add-data#update-data
void AddDataUpdateDocument(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::FieldValue;

  // To update some fields of a document without overwriting the entire
  // document, use the Update() method:
  DocumentReference washington_ref = db->Collection("cities").Document("DC");
  // Set the "capital" field of the city "DC".
  washington_ref.Update({{"capital", FieldValue::FromBoolean(true)}});

  // You can set a field in your document to a server timestamp which tracks
  // when the server receives the update.
  DocumentReference doc_ref = db->Collection("objects").Document("some-id");
  doc_ref.Update({{"timestamp", FieldValue::ServerTimestamp()}})
      .OnCompletion([](const Future<void>& future) {
        // ...
      });
}

// https://firebase.google.com/docs/firestore/manage-data/add-data#update_fields_in_nested_objects
void AddDataUpdateNestedObjects(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::FieldValue;

  // If your document contains nested objects, you can use "dot notation" to
  // reference nested fields within the document when you call Update():
  // Assume the document contains:
  // {
  //   name: "Frank",
  //   favorites: { food: "Pizza", color: "Blue", subject: "recess" }
  //   age: 12
  // }
  //
  // To update age and favorite color:
  db->Collection("users").Document("frank").Update({
      {"age", FieldValue::FromInteger(13)},
      {"favorites.color", FieldValue::FromString("red")},
  });
  // Dot notation allows you to update a single nested field without overwriting
  // other nested fields. If you update a nested field without dot notation, you
  // will overwrite the entire map field.
}

// https://firebase.google.com/docs/firestore/manage-data/transactions#batched-writes
void AddDataBatchedWrites(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::WriteBatch;

  // If you do not need to read any documents in your operation set, you can
  // execute multiple write operations as a single batch that contains any
  // combination of set(), update(), or delete() operations. A batch of writes
  // completes atomically and can write to multiple documents. The following
  // example shows how to build and commit a write batch:

  // Get a new write batch
  WriteBatch batch = db->batch();

  // Set the value of 'NYC'
  DocumentReference nyc_ref = db->Collection("cities").Document("NYC");
  batch.Set(nyc_ref, {});

  // Update the population of 'SF'
  DocumentReference sf_ref = db->Collection("cities").Document("SF");
  batch.Update(sf_ref, {{"population", FieldValue::FromInteger(1000000)}});

  // Delete the city 'LA'
  DocumentReference la_ref = db->Collection("cities").Document("LA");
  batch.Delete(la_ref);

  // Commit the batch
  batch.Commit().OnCompletion([](const Future<void>& future) {
    if (future.error() == Error::Ok) {
      std::cout << "Write batch success!\n";
    } else {
      std::cout << "Write batch failure: " << future.error_message() << '\n';
    }
  });
}

// https://firebase.google.com/docs/firestore/manage-data/transactions#transactions
void AddDataTransactions(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::Transaction;

  // The following example shows how to create and run a transaction:

  DocumentReference sf_doc_ref = db->Collection("cities").Document("SF");
  db->RunTransaction([sf_doc_ref](Transaction* transaction,
                                  std::string* out_error_message) -> Error {
      Error error = Error::Ok;

      DocumentSnapshot snapshot =
          transaction->Get(sf_doc_ref, &error, out_error_message);

      // Note: this could be done without a transaction by updating the
      // population using FieldValue::Increment().
      std::int64_t new_population =
          snapshot.Get("population").integer_value() + 1;
      transaction->Update(
          sf_doc_ref,
          {{"population", FieldValue::FromInteger(new_population)}});

      return Error::Ok;
    }).OnCompletion([](const Future<void>& future) {
    if (future.error() == Error::Ok) {
      std::cout << "Transaction success!\n";
    } else {
      std::cout << "Transaction failure: " << future.error_message() << '\n';
    }
  });
}

// https://firebase.google.com/docs/firestore/manage-data/delete-data#delete_documents
void AddDataDeleteDocuments(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::Error;

  // To delete a document, use the Delete() method:
  db->Collection("cities").Document("DC").Delete().OnCompletion(
      [](const Future<void>& future) {
        if (future.error() == Error::Ok) {
          std::cout << "DocumentSnapshot successfully deleted!\n";
        } else {
          std::cout << "Error deleting document: " << future.error_message()
                    << '\n';
        }
      });
  // WARNING: Deleting a document does not delete its subcollections!
}

// https://firebase.google.com/docs/firestore/manage-data/delete-data#fields
void AddDataDeleteFields(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::FieldValue;

  // To delete specific fields from a document, use the FieldValue.delete()
  // method when you update a document:
  DocumentReference doc_ref = db->Collection("cities").Document("BJ");
  doc_ref.Update({{"capital", FieldValue::Delete()}})
      .OnCompletion([](const Future<void>& future) { /*...*/ });

  // https://firebase.google.com/docs/firestore/manage-data/delete-data#collections
  // To delete an entire collection or subcollection in Cloud Firestore,
  // retrieve all the documents within the collection or subcollection and
  // delete them.
  // WARNING: deleting collections from a client SDK is not recommended.
}

// https://firebase.google.com/docs/firestore/query-data/get-data#example_data
void ReadDataExampleData(firebase::firestore::Firestore* db) {
  using firebase::firestore::CollectionReference;
  using firebase::firestore::FieldValue;

  // To get started, write some data about cities so we can look at different
  // ways to read it back:

  CollectionReference cities = db->Collection("cities");

  cities.Document("SF").Set({
      {"name", FieldValue::FromString("San Francisco")},
      {"state", FieldValue::FromString("CA")},
      {"country", FieldValue::FromString("USA")},
      {"capital", FieldValue::FromBoolean(false)},
      {"population", FieldValue::FromInteger(860000)},
      {"regions", FieldValue::FromArray({FieldValue::FromString("west_coast"),
                                         FieldValue::FromString("norcal")})},
  });

  cities.Document("LA").Set({
      {"name", FieldValue::FromString("Los Angeles")},
      {"state", FieldValue::FromString("CA")},
      {"country", FieldValue::FromString("USA")},
      {"capital", FieldValue::FromBoolean(false)},
      {"population", FieldValue::FromInteger(3900000)},
      {"regions", FieldValue::FromArray({FieldValue::FromString("west_coast"),
                                         FieldValue::FromString("socal")})},
  });

  cities.Document("DC").Set({
      {"name", FieldValue::FromString("Washington D.C.")},
      {"state", FieldValue::Null()},
      {"country", FieldValue::FromString("USA")},
      {"capital", FieldValue::FromBoolean(true)},
      {"population", FieldValue::FromInteger(680000)},
      {"regions",
       FieldValue::FromArray({FieldValue::FromString("east_coast")})},
  });

  cities.Document("TOK").Set({
      {"name", FieldValue::FromString("Tokyo")},
      {"state", FieldValue::Null()},
      {"country", FieldValue::FromString("Japan")},
      {"capital", FieldValue::FromBoolean(true)},
      {"population", FieldValue::FromInteger(9000000)},
      {"regions", FieldValue::FromArray({FieldValue::FromString("kanto"),
                                         FieldValue::FromString("honshu")})},
  });

  cities.Document("BJ").Set({
      {"name", FieldValue::FromString("Beijing")},
      {"state", FieldValue::Null()},
      {"country", FieldValue::FromString("China")},
      {"capital", FieldValue::FromBoolean(true)},
      {"population", FieldValue::FromInteger(21500000)},
      {"regions", FieldValue::FromArray({FieldValue::FromString("jingjinji"),
                                         FieldValue::FromString("hebei")})},
  });
}

// https://firebase.google.com/docs/firestore/query-data/get-data#get_a_document
void ReadDataGetDocument(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;

  // The following example shows how to retrieve the contents of a single
  // document using Get():
  DocumentReference doc_ref = db->Collection("cities").Document("SF");
  doc_ref.Get().OnCompletion([](const Future<DocumentSnapshot>& future) {
    if (future.error() == Error::Ok) {
      const DocumentSnapshot& document = *future.result();
      if (document.exists()) {
        std::cout << "DocumentSnapshot id: " << document.id() << '\n';
      } else {
        std::cout << "no such document\n";
      }
    } else {
      std::cout << "Get failed with: " << future.error_message() << '\n';
    }
  });
}

void ReadDataSourceOptions(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::Source;

  // You can set the source option to control how a get call uses the offline
  // cache.
  //
  // By default, a get call will attempt to fetch the latest document snapshot
  // from your database. On platforms with offline support, the client library
  // will use the offline cache if the network is unavailable or if the request
  // times out.
  //
  // You can specify the source option in a Get() call to change the default
  // behavior. You can fetch from only the database and ignore the offline
  // cache, or you can fetch from only the offline cache. For example:
  DocumentReference doc_ref = db->Collection("cities").Document("SF");
  Source source = Source::kCache;
  doc_ref.Get(source).OnCompletion([](const Future<DocumentSnapshot>& future) {
    if (future.error() == Error::Ok) {
      const DocumentSnapshot& document = *future.result();
      if (document.exists()) {
        std::cout << "Cached document id: " << document.id() << '\n';
      } else {
      }
    } else {
      std::cout << "Cached get failed: " << future.error_message() << '\n';
    }
  });
}

// https://firebase.google.com/docs/firestore/query-data/get-data#get_multiple_documents_from_a_collection
void ReadDataGetMultipleDocumentsFromCollection(
    firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::QuerySnapshot;

  // You can also retrieve multiple documents with one request by querying
  // documents in a collection. For example, you can use Where() to query for
  // all of the documents that meet a certain condition, then use Get() to
  // retrieve the results:
  db->Collection("cities")
      .WhereEqualTo("capital", FieldValue::FromBoolean(true))
      .Get()
      .OnCompletion([](const Future<QuerySnapshot>& future) {
        if (future.error() == Error::Ok) {
          for (const DocumentSnapshot& document :
               future.result()->documents()) {
            std::cout << document << '\n';
          }
        } else {
          std::cout << "Error getting documents: " << future.error_message()
                    << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/get-data#get_all_documents_in_a_collection
void ReadDataGetAllDocumentsInCollection(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::QuerySnapshot;

  // In addition, you can retrieve all documents in a collection by omitting the
  // Where() filter entirely:
  db->Collection("cities").Get().OnCompletion(
      [](const Future<QuerySnapshot>& future) {
        if (future.error() == Error::Ok) {
          for (const DocumentSnapshot& document :
               future.result()->documents()) {
            std::cout << document << '\n';
          }
        } else {
          std::cout << "Error getting documents: " << future.error_message()
                    << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/listen
void ReadDataListen(firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;

  // You can listen to a document with the AddSnapshotListener() method. An
  // initial call using the callback you provide creates a document snapshot
  // immediately with the current contents of the single document. Then, each
  // time the contents change, another call updates the document snapshot.
  DocumentReference doc_ref = db->Collection("cities").Document("SF");
  doc_ref.AddSnapshotListener(
      [](const DocumentSnapshot& snapshot, Error error) {
        if (error == Error::Ok) {
          if (snapshot.exists()) {
            std::cout << "Current data: " << snapshot << '\n';
          } else {
            std::cout << "Current data: null\n";
          }
        } else {
          std::cout << "Listen failed: " << error << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/listen#events-local-changes
void ReadDataEventsForLocalChanges(firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;

  // Local writes in your app will invoke snapshot listeners immediately. This
  // is because of an important feature called "latency compensation." When you
  // perform a write, your listeners will be notified with the new data before
  // the data is sent to the backend.
  //
  // Retrieved documents have metadata().has_pending_writes() property that
  // indicates whether the document has local changes that haven't been written
  // to the backend yet. You can use this property to determine the source of
  // events received by your snapshot listener:

  DocumentReference doc_ref = db->Collection("cities").Document("SF");
  doc_ref.AddSnapshotListener([](const DocumentSnapshot& snapshot,
                                 Error error) {
    if (error == Error::Ok) {
      const char* source =
          snapshot.metadata().has_pending_writes() ? "Local" : "Server";
      if (snapshot.exists()) {
        std::cout << source << " data: " << snapshot.Get("name").string_value()
                  << '\n';
      } else {
        std::cout << source << " data: null\n";
      }
    } else {
      std::cout << "Listen failed: " << error << '\n';
    }
  });
}

// https://firebase.google.com/docs/firestore/query-data/listen#events-metadata-changes
void ReadDataEventsForMetadataChanges(firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentReference;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::MetadataChanges;

  // When listening for changes to a document, collection, or query, you can
  // pass options to control the granularity of events that your listener will
  // receive.
  //
  // By default, listeners are not notified of changes that only affect
  // metadata. Consider what happens when your app writes a new document:
  //
  // A change event is immediately fired with the new data. The document has not
  // yet been written to the backend so the "pending writes" flag is true.
  // The document is written to the backend.
  // The backend notifies the client of the successful write. There is no change
  // to the document data, but there is a metadata change because the "pending
  // writes" flag is now false.
  // If you want to receive snapshot events when the document or query metadata
  // changes, pass a listen options object when attaching your listener:
  DocumentReference doc_ref = db->Collection("cities").Document("SF");
  doc_ref.AddSnapshotListener(
      MetadataChanges::kInclude,
      [](const DocumentSnapshot& snapshot, Error error) { /* ... */ });
}

// https://firebase.google.com/docs/firestore/query-data/listen#listen_to_multiple_documents_in_a_collection
void ReadDataListenToMultipleDocumentsInCollection(
    firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::QuerySnapshot;

  // As with documents, you can use AddSnapshotListener() instead of Get() to
  // listen to the results of a query. This creates a query snapshot. For
  // example, to listen to the documents with state CA:
  db->Collection("cities")
      .WhereEqualTo("state", FieldValue::FromString("CA"))
      .AddSnapshotListener([](const QuerySnapshot& snapshot, Error error) {
        if (error == Error::Ok) {
          std::vector<std::string> cities;
          std::cout << "Current cities in CA: " << error << '\n';
          for (const DocumentSnapshot& doc : snapshot.documents()) {
            cities.push_back(doc.Get("name").string_value());
            std::cout << "" << cities.back() << '\n';
          }
        } else {
          std::cout << "Listen failed: " << error << '\n';
        }
      });

  // The snapshot handler will receive a new query snapshot every time the query
  // results change (that is, when a document is added, removed, or modified).
}

// https://firebase.google.com/docs/firestore/query-data/listen#view_changes_between_snapshots
void ReadDataViewChangesBetweenSnapshots(firebase::firestore::Firestore* db) {
  using firebase::firestore::DocumentChange;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::QuerySnapshot;

  // It is often useful to see the actual changes to query results between query
  // snapshots, instead of simply using the entire query snapshot. For example,
  // you may want to maintain a cache as individual documents are added,
  // removed, and modified.
  db->Collection("cities")
      .WhereEqualTo("state", FieldValue::FromString("CA"))
      .AddSnapshotListener([](const QuerySnapshot& snapshot, Error error) {
        if (error == Error::Ok) {
          for (const DocumentChange& dc : snapshot.DocumentChanges()) {
            switch (dc.type()) {
              case DocumentChange::Type::kAdded:
                std::cout << "New city: "
                          << dc.document().Get("name").string_value() << '\n';
                break;
              case DocumentChange::Type::kModified:
                std::cout << "Modified city: "
                          << dc.document().Get("name").string_value() << '\n';
                break;
              case DocumentChange::Type::kRemoved:
                std::cout << "Removed city: "
                          << dc.document().Get("name").string_value() << '\n';
                break;
            }
          }
        } else {
          std::cout << "Listen failed: " << error << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/listen#detach_a_listener
void ReadDataDetachListener(firebase::firestore::Firestore* db) {
  using firebase::firestore::Error;
  using firebase::firestore::ListenerRegistration;
  using firebase::firestore::Query;
  using firebase::firestore::QuerySnapshot;

  // When you are no longer interested in listening to your data, you must
  // detach your listener so that your event callbacks stop getting called. This
  // allows the client to stop using bandwidth to receive updates. For example:
  Query query = db->Collection("cities");
  ListenerRegistration registration = query.AddSnapshotListener(
      [](const QuerySnapshot& snapshot, Error error) { /* ... */ });
  // Stop listening to changes
  registration.Remove();

  // A listen may occasionally fail — for example, due to security permissions,
  // or if you tried to listen on an invalid query. After an error, the listener
  // will not receive any more events, and there is no need to detach your
  // listener.
}

// https://firebase.google.com/docs/firestore/query-data/queries#simple_queries
void ReadDataSimpleQueries(firebase::firestore::Firestore* db) {
  using firebase::firestore::CollectionReference;
  using firebase::firestore::FieldValue;
  using firebase::firestore::Query;

  // Cloud Firestore provides powerful query functionality for specifying which
  // documents you want to retrieve from a collection.

  // The following query returns all cities with state CA:
  CollectionReference cities_ref = db->Collection("cities");
  // Create a query against the collection.
  Query query_ca =
      cities_ref.WhereEqualTo("state", FieldValue::FromString("CA"));

  // The following query returns all the capital cities:
  Query capital_cities = db->Collection("cities").WhereEqualTo(
      "capital", FieldValue::FromBoolean(true));
}

// https://firebase.google.com/docs/firestore/query-data/queries#execute_a_query
void ReadDataExecuteQuery(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::FieldValue;
  using firebase::firestore::QuerySnapshot;

  // After creating a query object, use the Get() function to retrieve the
  // results:
  db->Collection("cities")
      .WhereEqualTo("capital", FieldValue::FromBoolean(true))
      .Get()
      .OnCompletion([](const Future<QuerySnapshot>& future) {
        if (future.error() == Error::Ok) {
          for (const DocumentSnapshot& document :
               future.result()->documents()) {
            std::cout << document << '\n';
          }
        } else {
          std::cout << "Error getting documents: " << future.error_message()
                    << '\n';
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/queries#query_operators
void ReadDataQueryOperators(firebase::firestore::Firestore* db) {
  using firebase::firestore::CollectionReference;
  using firebase::firestore::FieldValue;

  CollectionReference cities_ref = db->Collection("cities");

  // Some example filters:
  cities_ref.WhereEqualTo("state", FieldValue::FromString("CA"));
  cities_ref.WhereLessThan("population", FieldValue::FromInteger(100000));
  cities_ref.WhereGreaterThanOrEqualTo("name",
                                       FieldValue::FromString("San Francisco"));
}

// https://firebase.google.com/docs/firestore/query-data/queries#compound_queries
void ReadDataCompoundQueries(firebase::firestore::Firestore* db) {
  using firebase::firestore::CollectionReference;
  using firebase::firestore::FieldValue;

  CollectionReference cities_ref = db->Collection("cities");

  // You can also chain multiple where() methods to create more specific queries
  // (logical AND). However, to combine the equality operator (==) with a range
  // (<, <=, >, >=) or array-contains clause, make sure to create a composite
  // index.
  cities_ref.WhereEqualTo("state", FieldValue::FromString("CO"))
      .WhereEqualTo("name", FieldValue::FromString("Denver"));
  cities_ref.WhereEqualTo("state", FieldValue::FromString("CA"))
      .WhereLessThan("population", FieldValue::FromInteger(1000000));

  // You can only perform range comparisons (<, <=, >, >=) on a single field,
  // and you can include at most one array-contains clause in a compound query:
  cities_ref.WhereGreaterThanOrEqualTo("state", FieldValue::FromString("CA"))
      .WhereLessThanOrEqualTo("state", FieldValue::FromString("IN"));
  cities_ref.WhereEqualTo("state", FieldValue::FromString("CA"))
      .WhereGreaterThan("population", FieldValue::FromInteger(1000000));

  // BAD EXAMPLE -- will crash the program:
  // cities_ref.WhereGreaterThanOrEqualTo("state", FieldValue::FromString("CA"))
  //     .WhereGreaterThan("population",
  //                       FieldValue::FromInteger(100000));
}

// https://firebase.google.com/docs/firestore/query-data/order-limit-data#order_and_limit_data
void ReadDataOrderAndLimitData(firebase::firestore::Firestore* db) {
  using firebase::firestore::CollectionReference;
  using firebase::firestore::FieldValue;
  using firebase::firestore::Query;

  CollectionReference cities_ref = db->Collection("cities");

  // By default, a query retrieves all documents that satisfy the query in
  // ascending order by document ID. You can specify the sort order for your
  // data using OrderBy(), and you can limit the number of documents retrieved
  // using Limit().
  //
  // Note: An OrderBy() clause also filters for existence of the given field.
  // The result set will not include documents that do not contain the given
  // field.
  //
  // For example, you could query for the first 3 cities alphabetically with:
  cities_ref.OrderBy("name").Limit(3);

  // You could also sort in descending order to get the last 3 cities:
  cities_ref.OrderBy("name", Query::Direction::kDescending).Limit(3);

  // You can also order by multiple fields. For example, if you wanted to order
  // by state, and within each state order by population in descending order:
  cities_ref.OrderBy("state").OrderBy("name", Query::Direction::kDescending);

  // You can combine Where() filters with OrderBy() and Limit(). In the
  // following example, the queries define a population threshold, sort by
  // population in ascending order, and return only the first few results that
  // exceed the threshold:
  cities_ref.WhereGreaterThan("population", FieldValue::FromInteger(100000))
      .OrderBy("population")
      .Limit(2);

  // However, if you have a filter with a range comparison (<, <=, >, >=), your
  // first ordering must be on the same field.
  // BAD EXAMPLE -- will crash the program:
  // cities_ref.WhereGreaterThan("population", FieldValue::FromInteger(100000))
  //     .OrderBy("country");
}

// https://firebase.google.com/docs/firestore/query-data/query-cursors#add_a_simple_cursor_to_a_query
void ReadDataAddSimpleCursorToQuery(firebase::firestore::Firestore* db) {
  using firebase::firestore::FieldValue;

  // Use the StartAt() or StartAfter() methods to define the start point for
  // a query. The StartAt() method includes the start point, while the
  // StartAfter() method excludes it.
  //
  // For example, if you use StartAt(FieldValue::FromString("A")) in a query, it
  // returns the entire alphabet. If you use
  // StartAftert(FieldValue::FromString("A")) instead, it returns B-Z.

  // Get all cities with a population >= 1,000,000, ordered by population,
  db->Collection("cities")
      .OrderBy("population")
      .StartAt({FieldValue::FromInteger(1000000)});

  // Similarly, use the EndAt() or EndBefore() methods to define an end point
  // for your query results.
  // Get all cities with a population <= 1,000,000, ordered by population,
  db->Collection("cities")
      .OrderBy("population")
      .EndAt({FieldValue::FromInteger(1000000)});
}

// https://firebase.google.com/docs/firestore/query-data/query-cursors#use_a_document_snapshot_to_define_the_query_cursor
void ReadDataDocumentSnapshotInCursor(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::Query;

  // You can also pass a document snapshot to the cursor clause as the start or
  // end point of the query cursor. The values in the document snapshot serve as
  // the values in the query cursor.
  //
  // For example, take a snapshot of a "San Francisco" document in your data set
  // of cities and populations. Then, use that document snapshot as the start
  // point for your population query cursor. Your query will return all the
  // cities with a population larger than or equal to San Francisco's, as
  // defined in the document snapshot.
  db->Collection("cities").Document("SF").Get().OnCompletion(
      [db](const Future<DocumentSnapshot>& future) {
        if (future.error() == Error::Ok) {
          const DocumentSnapshot& document_snapshot = *future.result();
          Query bigger_than_sf = db->Collection("cities")
                                     .OrderBy("population")
                                     .StartAt({document_snapshot});
          // ...
        }
      });
}

// https://firebase.google.com/docs/firestore/query-data/query-cursors#paginate_a_query
void ReadDataPaginateQuery(firebase::firestore::Firestore* db) {
  using firebase::Future;
  using firebase::firestore::DocumentSnapshot;
  using firebase::firestore::Error;
  using firebase::firestore::Query;
  using firebase::firestore::QuerySnapshot;

  // Paginate queries by combining query cursors with the Limit() method. For
  // example, use the last document in a batch as the start of a cursor for the
  // next batch.

  // Construct query for first 25 cities, ordered by population
  Query first = db->Collection("cities").OrderBy("population").Limit(25);

  first.Get().OnCompletion([db](const Future<QuerySnapshot>& future) {
    if (future.error() != Error::Ok) {
      // Handle error...
      return;
    }

    // Get the last visible document
    const QuerySnapshot& document_snapshots = *future.result();
    std::vector<DocumentSnapshot> documents = document_snapshots.documents();
    const DocumentSnapshot& last_visible = documents.back();

    // Construct a new query starting at this document,
    // get the next 25 cities.
    Query next = db->Collection("cities")
                     .OrderBy("population")
                     .StartAfter(last_visible)
                     .Limit(25);

    // Use the query for pagination
    // ...
  });
}

}  // namespace snippets

void RunAllSnippets(firebase::firestore::Firestore* db) {
  snippets::QuickstartAddData(db);
  snippets::QuickstartReadData(db);

  snippets::AddDataSetDocument(db);
  snippets::AddDataDataTypes(db);
  snippets::AddDataAddDocument(db);
  snippets::AddDataUpdateDocument(db);
  snippets::AddDataUpdateNestedObjects(db);
  snippets::AddDataBatchedWrites(db);
  snippets::AddDataTransactions(db);
  snippets::AddDataDeleteDocuments(db);
  snippets::AddDataDeleteFields(db);

  snippets::ReadDataExampleData(db);
  snippets::ReadDataGetDocument(db);
  snippets::ReadDataSourceOptions(db);
  snippets::ReadDataGetMultipleDocumentsFromCollection(db);
  snippets::ReadDataGetAllDocumentsInCollection(db);

  snippets::ReadDataListen(db);
  snippets::ReadDataEventsForLocalChanges(db);
  snippets::ReadDataEventsForMetadataChanges(db);
  snippets::ReadDataListenToMultipleDocumentsInCollection(db);
  snippets::ReadDataViewChangesBetweenSnapshots(db);
  snippets::ReadDataDetachListener(db);

  snippets::ReadDataSimpleQueries(db);
  snippets::ReadDataExecuteQuery(db);
  snippets::ReadDataQueryOperators(db);
  snippets::ReadDataCompoundQueries(db);

  snippets::ReadDataOrderAndLimitData(db);

  snippets::ReadDataAddSimpleCursorToQuery(db);

  snippets::ReadDataDocumentSnapshotInCursor(db);
  snippets::ReadDataPaginateQuery(db);
}
